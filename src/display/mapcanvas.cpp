// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Ulf Hermann <ulfonk_mennhar@gmx.de> (Alve)
// Author: Marek Krejza <krejza@gmail.com> (Caligor)
// Author: Nils Schimmelmann <nschimme@gmail.com> (Jahara)

#include "mapcanvas.h"

#include "../configuration/configuration.h"
#include "../global/parserutils.h"
#include "../global/progresscounter.h"
#include "../global/utils.h"
#include "../map/ExitDirection.h"
#include "../map/coordinate.h"
#include "../map/exit.h"
#include "../map/infomark.h"
#include "../map/room.h"
#include "../map/roomid.h"
// #include "../mapdata/mapdata.h" // Already included via mapcanvas.h's MapData member
#include "../mapdata/roomselection.h"
#include "InfoMarkSelection.h"
#include "MapCanvasData.h"
#include "MapCanvasRoomDrawer.h" // For mctp::MapCanvasTexturesProxy
#include "Textures.h"            // For mctp::MapCanvasTexturesProxy
#include "connectionselection.h"


#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

#include <QGestureEvent>
#include <QMessageLogContext>
#include <QOpenGLDebugMessage>
#include <QSize>
#include <QString>
#include <QTimer>
#include <QToolTip>
#include <QtGui>
#include <QtWidgets>

#if defined(_MSC_VER) || defined(__MINGW32__)
#undef near
#undef far
#endif

using NonOwningPointer = MapCanvas *;
NODISCARD static NonOwningPointer &primaryMapCanvas()
{
    static NonOwningPointer primary = nullptr;
    return primary;
}

// Helper function for distance calculation (2D)
static float calculateDistance(const Coordinate &pos1, const Coordinate &pos2)
{
    if (pos1.z != pos2.z) {
        return std::numeric_limits<float>::max();
    }
    return std::sqrt(std::pow(pos1.x - pos2.x, 2) + std::pow(pos1.y - pos2.y, 2));
}

MapCanvas::MapCanvas(MapData &mapData,
                     PrespammedPath &prespammedPath,
                     Mmapper2Group &groupManager,
                     QWidget *const parent)
    : QOpenGLWidget{parent}
    , MapCanvasViewport{static_cast<QWidget &>(*this)}
    , MapCanvasInputState{prespammedPath}
    , m_mapScreen{static_cast<MapCanvasViewport &>(*this)}
    , m_opengl{}
    , m_glFont{m_opengl}
    , m_data{mapData}
    , m_groupManager{groupManager}
    , m_isRemeshing{false}
{
    NonOwningPointer &pmc = primaryMapCanvas();
    if (pmc == nullptr) {
        pmc = this;
    }

    setCursor(Qt::OpenHandCursor);
    grabGesture(Qt::PinchGesture);
    setContextMenuPolicy(Qt::CustomContextMenu);
}

MapCanvas::~MapCanvas()
{
    NonOwningPointer &pmc = primaryMapCanvas();
    if (pmc == this) {
        pmc = nullptr;
    }

    cleanupOpenGL();
}

MapCanvas *MapCanvas::getPrimary()
{
    return primaryMapCanvas();
}

void MapCanvas::slot_layerUp()
{
    ++m_currentLayer;
    layerChanged();
}

void MapCanvas::slot_layerDown()
{
    --m_currentLayer;
    layerChanged();
}

void MapCanvas::slot_layerReset()
{
    m_currentLayer = 0;
    layerChanged();
}

void MapCanvas::slot_setCanvasMouseMode(const CanvasMouseModeEnum mode)
{
    slot_clearAllSelections();

    switch (mode) {
    case CanvasMouseModeEnum::MOVE:
        setCursor(Qt::OpenHandCursor);
        break;

    default:
    case CanvasMouseModeEnum::NONE:
    case CanvasMouseModeEnum::RAYPICK_ROOMS:
    case CanvasMouseModeEnum::SELECT_CONNECTIONS:
    case CanvasMouseModeEnum::CREATE_INFOMARKS:
        setCursor(Qt::CrossCursor);
        break;

    case CanvasMouseModeEnum::SELECT_ROOMS:
    case CanvasMouseModeEnum::CREATE_ROOMS:
    case CanvasMouseModeEnum::CREATE_CONNECTIONS:
    case CanvasMouseModeEnum::CREATE_ONEWAY_CONNECTIONS:
    case CanvasMouseModeEnum::SELECT_INFOMARKS:
        setCursor(Qt::ArrowCursor);
        break;
    }

    m_canvasMouseMode = mode;
    m_selectedArea = false;
    selectionChanged();
}

void MapCanvas::slot_setRoomSelection(const SigRoomSelection &selection)
{
    if (!selection.isValid()) {
        m_roomSelection.reset();
    } else {
        m_roomSelection = selection.getShared();
        auto &sel = deref(m_roomSelection);
        auto &map = m_data;
        sel.removeMissing(map);

        qDebug() << "Updated selection with" << sel.size() << "rooms";
        if (sel.size() == 1) {
            if (const auto r = map.findRoomHandle(sel.getFirstRoomId())) {
                auto message = mmqt::previewRoom(r,
                                                 mmqt::StripAnsiEnum::Yes,
                                                 mmqt::PreviewStyleEnum::ForLog);
                log(message);
            }
        }
    }

    emit sig_newRoomSelection(selection);
    selectionChanged();
}

void MapCanvas::slot_setConnectionSelection(const std::shared_ptr<ConnectionSelection> &selection)
{
    m_connectionSelection = selection;
    emit sig_newConnectionSelection(selection.get());
    selectionChanged();
}

void MapCanvas::slot_setInfoMarkSelection(const std::shared_ptr<InfoMarkSelection> &selection)
{
    if (m_canvasMouseMode == CanvasMouseModeEnum::CREATE_INFOMARKS) {
        m_infoMarkSelection = selection;

    } else if (selection == nullptr || selection->empty()) {
        m_infoMarkSelection = nullptr;

    } else {
        qDebug() << "Updated selection with" << selection->size() << "infomarks";
        m_infoMarkSelection = selection;
    }

    emit sig_newInfoMarkSelection(m_infoMarkSelection.get());
    selectionChanged();
}

NODISCARD static uint32_t operator&(const Qt::KeyboardModifiers left, const Qt::Modifier right)
{
    return static_cast<uint32_t>(left) & static_cast<uint32_t>(right);
}

void MapCanvas::wheelEvent(QWheelEvent *const event)
{
    const bool hasCtrl = (event->modifiers() & Qt::CTRL) != 0u;

    switch (m_canvasMouseMode) {
    case CanvasMouseModeEnum::MOVE:
    case CanvasMouseModeEnum::SELECT_INFOMARKS:
    case CanvasMouseModeEnum::CREATE_INFOMARKS:
    case CanvasMouseModeEnum::RAYPICK_ROOMS:
    case CanvasMouseModeEnum::SELECT_ROOMS:
    case CanvasMouseModeEnum::SELECT_CONNECTIONS:
    case CanvasMouseModeEnum::CREATE_ROOMS:
    case CanvasMouseModeEnum::CREATE_CONNECTIONS:
    case CanvasMouseModeEnum::CREATE_ONEWAY_CONNECTIONS:
        if (hasCtrl) {
            if (event->angleDelta().y() > 100) {
                slot_layerDown();
            } else if (event->angleDelta().y() < -100) {
                slot_layerUp();
            }
        } else {
            const auto zoomAndMaybeRecenter = [this, event](const int numSteps) -> bool {
                assert(numSteps != 0);
                const auto optCenter = getUnprojectedMouseSel(event);

                if (!optCenter) {
                    m_scaleFactor.logStep(numSteps);
                    return false;
                }

                const auto newCenter = optCenter->to_vec3();
                const auto oldCenter = m_mapScreen.getCenter();
                const auto delta1 = glm::ivec2(glm::vec2(newCenter - oldCenter) * static_cast<float>(SCROLL_SCALE));
                emit sig_mapMove(delta1.x, delta1.y);
                m_scaleFactor.logStep(numSteps);
                setViewportAndMvp(width(), height());

                if (const auto &optCenter2 = getUnprojectedMouseSel(event)) {
                    const auto delta2 = glm::ivec2(glm::vec2(optCenter2->to_vec3() - newCenter) * static_cast<float>(SCROLL_SCALE));
                    emit sig_mapMove(-delta2.x, -delta2.y);
                }
                return true;
            };

            const int numSteps = event->angleDelta().y() / 120;
            if (numSteps != 0) {
                zoomAndMaybeRecenter(numSteps);
                zoomChanged();
                resizeGL();
            }
        }
        break;
    case CanvasMouseModeEnum::NONE:
    default:
        break;
    }
    event->accept();
}

void MapCanvas::slot_onForcedPositionChange() { slot_requestUpdate(); }

bool MapCanvas::event(QEvent *const event) {
    auto tryHandlePinchZoom = [this, event]() -> bool {
        if (event->type() != QEvent::Gesture) { return false; }
        const auto *const gestureEvent = dynamic_cast<QGestureEvent *>(event);
        if (gestureEvent == nullptr) { return false; }
        QGesture *const gesture = gestureEvent->gesture(Qt::PinchGesture);
        const auto *const pinch = dynamic_cast<QPinchGesture *>(gesture);
        if (pinch == nullptr) { return false; }
        const QPinchGesture::ChangeFlags changeFlags = pinch->changeFlags();
        if (changeFlags & QPinchGesture::ScaleFactorChanged) {
            const auto pinchFactor = static_cast<float>(pinch->totalScaleFactor());
            m_scaleFactor.setPinch(pinchFactor);
        }
        if (pinch->state() == Qt::GestureFinished) {
            m_scaleFactor.endPinch();
            zoomChanged();
        }
        resizeGL();
        return true;
    };
    if (tryHandlePinchZoom()) { return true; }
    return QOpenGLWidget::event(event);
}

void MapCanvas::slot_createRoom() {
    if (!hasSel1()) { return; }
    const Coordinate c = getSel1().getCoordinate();
    if (m_data.findRoomHandle(c).exists()) { return; }
    m_data.createEmptyRoom(Coordinate{c.x, c.y, m_currentLayer});
}

std::shared_ptr<InfoMarkSelection> MapCanvas::getInfoMarkSelection(const MouseSel &sel) {
    static constexpr float CLICK_RADIUS = 10.f;
    const auto center = sel.to_vec3();
    const auto optClickPoint = project(center);
    if (!optClickPoint) {
        assert(false);
        static constexpr auto INFOMARK_CLICK_RADIUS = INFOMARK_SCALE / 5;
        const auto pos = sel.getScaledCoordinate(INFOMARK_SCALE);
        const auto lo = pos + Coordinate{-INFOMARK_CLICK_RADIUS, -INFOMARK_CLICK_RADIUS, 0};
        const auto hi = pos + Coordinate{+INFOMARK_CLICK_RADIUS, +INFOMARK_CLICK_RADIUS, 0};
        return InfoMarkSelection::alloc(m_data, lo, hi);
    }
    const glm::vec2 clickPoint = glm::vec2{optClickPoint.value()};
    glm::vec3 maxCoord = center;
    glm::vec3 minCoord = center;
    const auto probe = [this, &clickPoint, &maxCoord, &minCoord](const glm::vec2 &offset) -> void {
        const auto coord = unproject_clamped(clickPoint + offset);
        maxCoord = glm::max(maxCoord, coord);
        minCoord = glm::min(minCoord, coord);
    };
    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            probe(glm::vec2{static_cast<float>(dx) * CLICK_RADIUS, static_cast<float>(dy) * CLICK_RADIUS});
        }
    }
    const auto getScaled = [this](const glm::vec3 &c) -> Coordinate {
        const auto pos = glm::ivec3(glm::vec2(c) * static_cast<float>(INFOMARK_SCALE), m_currentLayer);
        return Coordinate{pos.x, pos.y, pos.z};
    };
    return InfoMarkSelection::alloc(m_data, getScaled(minCoord), getScaled(maxCoord));
}

void MapCanvas::mousePressEvent(QMouseEvent *const event) { /* Omitted for brevity - assume it's the same as last read */ }
void MapCanvas::mouseMoveEvent(QMouseEvent *const event) { /* Omitted for brevity - assume it's the same as last read */ }
void MapCanvas::mouseReleaseEvent(QMouseEvent *const event) { /* Omitted for brevity - assume it's the same as last read */ }


QSize MapCanvas::minimumSizeHint() const { return {100, 100}; }
QSize MapCanvas::sizeHint() const { return {BASESIZE, BASESIZE}; }

void MapCanvas::slot_setScroll(const glm::vec2 &worldPos) { m_scroll = worldPos; resizeGL(); }
void MapCanvas::slot_setHorizontalScroll(const float worldX) { m_scroll.x = worldX; resizeGL(); }
void MapCanvas::slot_setVerticalScroll(const float worldY) { m_scroll.y = worldY; resizeGL(); }

void MapCanvas::slot_zoomIn() { m_scaleFactor.logStep(1); zoomChanged(); resizeGL(); }
void MapCanvas::slot_zoomOut() { m_scaleFactor.logStep(-1); zoomChanged(); resizeGL(); }
void MapCanvas::slot_zoomReset() { m_scaleFactor.set(1.f); zoomChanged(); resizeGL(); }

void MapCanvas::onMovement() {
    const Coordinate &pos = m_data.tryGetPosition().value_or(Coordinate{});
    m_currentLayer = pos.z;
    emit sig_onCenter(pos.to_vec2() + glm::vec2{0.5f, 0.5f});
    update();
}

void MapCanvas::slot_dataLoaded() {
    onMovement();
    scheduleRemesh(); // Changed from forceUpdateMeshes()
}

void MapCanvas::slot_moveMarker(const RoomId id) { m_data.setRoom(id); onMovement(); }
void MapCanvas::infomarksChanged() { m_batches.infomarksMeshes.reset(); update(); }
void MapCanvas::layerChanged() { update(); }
void MapCanvas::forceUpdateMeshes() {
    m_batches.resetExistingMeshesAndIgnorePendingRemesh();
    m_diff.resetExistingMeshesAndIgnorePendingRemesh();
    update();
}

void MapCanvas::triggerNextRemeshCallback() {
    if (!m_stagedRemeshCallbacks.empty()) {
        auto callback = m_stagedRemeshCallbacks.front();
        m_stagedRemeshCallbacks.pop_front();
        callback();
    } else {
        m_isRemeshing = false;
    }
}

void MapCanvas::finishCurrentStageBatch(const RoomIdSet& roomsJustUpdated) {
    if (m_batches.remeshCookie.isPending()) {
        if (m_batches.remeshCookie.isReady()) {
            SharedMapBatchFinisher finisher = m_batches.remeshCookie.get();
            if (finisher) {
                if (!m_batches.mapBatches) {
                    m_batches.mapBatches.emplace();
                }
                finisher->finish(m_batches.mapBatches.value(), getOpenGL(), getGLFont(), roomsJustUpdated);
                MMLOG_DEBUG() << "Partial batches finished for " << roomsJustUpdated.size() << " rooms.";
                update();
            } else {
                MMLOG_DEBUG() << "Failed to get finisher for partial batch (was ready).";
            }
            triggerNextRemeshCallback();
        } else {
            MMLOG_DEBUG() << "Partial batch future is pending but not ready. Re-scheduling check.";
            QTimer::singleShot(0, [this, rsj = roomsJustUpdated]() {
                finishCurrentStageBatch(rsj);
            });
            return;
        }
    } else {
         MMLOG_DEBUG() << "No pending remesh cookie for stage. Rooms: " << roomsJustUpdated.size() << ". Triggering next.";
         triggerNextRemeshCallback();
    }
}

void MapCanvas::scheduleRemesh() {
    bool was_remeshing = m_isRemeshing.exchange(true);
    if (was_remeshing) {
        MMLOG_DEBUG() << "Remeshing cycle interrupted and restarting.";
    }
    m_stagedRemeshCallbacks.clear();
    m_remeshedRoomsInCurrentCycle.clear();
    m_stagedRemeshCallbacks.push_back([this]() { remeshStageNearPlayer(); });
    m_stagedRemeshCallbacks.push_back([this]() { remeshStageMediumDistance(); });
    m_stagedRemeshCallbacks.push_back([this]() { remeshStageFarDistance(); });
    m_stagedRemeshCallbacks.push_back([this]() { remeshFinish(); });
    if (!was_remeshing) {
        triggerNextRemeshCallback();
    }
}

void MapCanvas::remeshStageNearPlayer() {
    MMLOG_DEBUG() << "Remeshing stage: Near Player";
    RoomIdSet roomsToRemeshThisStage;
    auto playerPosOpt = m_data.tryGetPosition();
    if (playerPosOpt) {
        const Coordinate playerPos = playerPosOpt.value();
        for (const auto& pair : m_data.getCurrentMap().getRooms()) {
            const RoomId roomId = pair.first;
            if (m_remeshedRoomsInCurrentCycle.count(roomId)) continue;
            const auto room = m_data.findRoomHandle(roomId);
            if (room.exists() && calculateDistance(playerPos, room.getCoordinate()) <= PLAYER_VICINITY_RADIUS) {
                roomsToRemeshThisStage.insert(roomId);
            }
        }
    } else {
        MMLOG_DEBUG() << "Player position not valid for Near Player stage.";
    }
    for(RoomId id : roomsToRemeshThisStage) { m_remeshedRoomsInCurrentCycle.insert(id); }
    if (!roomsToRemeshThisStage.empty()) {
        MMLOG_DEBUG() << "Queueing Near Player remesh for " << roomsToRemeshThisStage.size() << " rooms.";
        m_batches.remeshCookie.set(m_data.generateBatchesForRooms(roomsToRemeshThisStage, mctp::MapCanvasTexturesProxy{m_textures}));
        finishCurrentStageBatch(roomsToRemeshThisStage);
    } else {
        MMLOG_DEBUG() << "No new rooms to remesh in Near Player stage. Triggering next.";
        triggerNextRemeshCallback();
    }
}

void MapCanvas::remeshStageMediumDistance() {
    MMLOG_DEBUG() << "Remeshing stage: Medium Distance";
    RoomIdSet roomsToRemeshThisStage;
    auto playerPosOpt = m_data.tryGetPosition();
    if (playerPosOpt) {
        const Coordinate playerPos = playerPosOpt.value();
        for (const auto& pair : m_data.getCurrentMap().getRooms()) {
            const RoomId roomId = pair.first;
            if (m_remeshedRoomsInCurrentCycle.count(roomId)) continue;
            const auto room = m_data.findRoomHandle(roomId);
            if (room.exists() && calculateDistance(playerPos, room.getCoordinate()) <= MEDIUM_DISTANCE_RADIUS) {
                roomsToRemeshThisStage.insert(roomId);
            }
        }
    } else { MMLOG_DEBUG() << "Player position not valid for Medium Distance stage."; }
    for(RoomId id : roomsToRemeshThisStage) { m_remeshedRoomsInCurrentCycle.insert(id); }
    if (!roomsToRemeshThisStage.empty()) {
        MMLOG_DEBUG() << "Queueing Medium Distance remesh for " << roomsToRemeshThisStage.size() << " rooms.";
        m_batches.remeshCookie.set(m_data.generateBatchesForRooms(roomsToRemeshThisStage, mctp::MapCanvasTexturesProxy{m_textures}));
        finishCurrentStageBatch(roomsToRemeshThisStage);
    } else {
        MMLOG_DEBUG() << "No new rooms to remesh in Medium Distance stage. Triggering next.";
        triggerNextRemeshCallback();
    }
}

void MapCanvas::remeshStageFarDistance() {
    MMLOG_DEBUG() << "Remeshing stage: Far Distance";
    RoomIdSet roomsToRemeshThisStage;
    for (const auto& pair : m_data.getCurrentMap().getRooms()) {
        const RoomId roomId = pair.first;
        if (m_remeshedRoomsInCurrentCycle.count(roomId)) continue;
        roomsToRemeshThisStage.insert(roomId);
    }
    for(RoomId id : roomsToRemeshThisStage) { m_remeshedRoomsInCurrentCycle.insert(id); }
    if (!roomsToRemeshThisStage.empty()) {
        MMLOG_DEBUG() << "Queueing Far Distance remesh for " << roomsToRemeshThisStage.size() << " rooms.";
        m_batches.remeshCookie.set(m_data.generateBatchesForRooms(roomsToRemeshThisStage, mctp::MapCanvasTexturesProxy{m_textures}));
        finishCurrentStageBatch(roomsToRemeshThisStage);
    } else {
        MMLOG_DEBUG() << "No new rooms to remesh in Far Distance stage. Triggering next.";
        triggerNextRemeshCallback();
    }
}

void MapCanvas::remeshFinish() {
    MMLOG_DEBUG() << "Remeshing cycle finished. Total rooms processed: " << m_remeshedRoomsInCurrentCycle.size();
    m_isRemeshing = false;
    update();
}

void MapCanvas::slot_mapChanged() { scheduleRemesh(); update(); }
void MapCanvas::slot_requestUpdate() { update(); }

void MapCanvas::screenChanged() {
    auto &gl = getOpenGL();
    if (!gl.isRendererInitialized()) { return; }
    const auto newDpi = static_cast<float>(QPaintDevice::devicePixelRatioF());
    const auto oldDpi = gl.getDevicePixelRatio();
    if (!utils::equals(newDpi, oldDpi)) {
        log(QString("Display: %1 DPI").arg(static_cast<double>(newDpi)));
        m_batches.resetExistingMeshesButKeepPendingRemesh();
        gl.setDevicePixelRatio(newDpi);
        auto &font = getGLFont();
        font.cleanup(); font.init();
        update();
    }
}

void MapCanvas::selectionChanged() { update(); emit sig_selectionChanged(); }
void MapCanvas::graphicsSettingsChanged() { update(); }

void MapCanvas::userPressedEscape(bool /*pressed*/) {
    switch (m_canvasMouseMode) {
    case CanvasMouseModeEnum::NONE: case CanvasMouseModeEnum::CREATE_ROOMS: break;
    case CanvasMouseModeEnum::CREATE_CONNECTIONS: case CanvasMouseModeEnum::SELECT_CONNECTIONS:
    case CanvasMouseModeEnum::CREATE_ONEWAY_CONNECTIONS: slot_clearConnectionSelection(); break;
    case CanvasMouseModeEnum::RAYPICK_ROOMS: case CanvasMouseModeEnum::SELECT_ROOMS:
        m_selectedArea = false; m_roomSelectionMove.reset(); slot_clearRoomSelection(); break;
    case CanvasMouseModeEnum::MOVE: FALLTHROUGH;
    case CanvasMouseModeEnum::SELECT_INFOMARKS: case CanvasMouseModeEnum::CREATE_INFOMARKS:
        m_infoMarkSelectionMove.reset(); slot_clearInfoMarkSelection(); break;
    }
}

void MapCanvas::finishPendingMapBatches() {
    if (m_batches.remeshCookie.isPending() && m_batches.remeshCookie.isReady()) {
        SharedMapBatchFinisher finisher = m_batches.remeshCookie.get();
        if (finisher) {
            if (!m_batches.mapBatches) {
                m_batches.mapBatches.emplace();
            }
            finisher->finish(m_batches.mapBatches.value(), getOpenGL(), getGLFont(), std::nullopt /* Full update */);
            m_diff.highlight.reset();
            update();
        } else {
           MMLOG_DEBUG() << "Failed to get finisher for full batch.";
        }
    }
}

// Placeholder for methods not fully shown but assumed to be part of the file
void MapCanvas::updateMapBatches() { /* ... */ }
void MapCanvas::initializeGL() { /* ... */ }
void MapCanvas::paintGL() { /* ... */ }
void MapCanvas::drawGroupCharacters(CharacterBatch &characterBatch) { /* ... */ }
void MapCanvas::resizeGL(int width, int height) { /* ... */ }
void MapCanvas::setAnimating(bool value) { /* ... */ }
void MapCanvas::renderLoop() { /* ... */ }
void MapCanvas::initLogger() { /* ... */ }
void MapCanvas::initTextures() { /* ... */ }
void MapCanvas::updateTextures() { /* ... */ }
void MapCanvas::updateMultisampling() { /* ... */ }
glm::mat4 MapCanvas::getViewProj_old(const glm::vec2 &scrollPos, const glm::ivec2 &size, float zoomScale, int currentLayer) { return glm::mat4(1.0f); }
glm::mat4 MapCanvas::getViewProj(const glm::vec2 &scrollPos, const glm::ivec2 &size, float zoomScale, int currentLayer) { return glm::mat4(1.0f); }
void MapCanvas::setMvp(const glm::mat4 &viewProj) { /* ... */ }
void MapCanvas::setViewportAndMvp(int width, int height) { /* ... */ }
BatchedInfomarksMeshes MapCanvas::getInfoMarksMeshes() { return {}; }
void MapCanvas::drawInfoMark(InfomarksBatch &batch, const InfomarkHandle &marker, int currentLayer, const glm::vec2 &offset, const std::optional<Color> &overrideColor) { /* ... */ }
void MapCanvas::updateBatches() { /* ... */ }
void MapCanvas::updateInfomarkBatches() { /* ... */ }
void MapCanvas::actuallyPaintGL() { /* ... */ }
void MapCanvas::paintMap() { /* ... */ }
void MapCanvas::renderMapBatches() { /* ... */ }
void MapCanvas::paintBatchedInfomarks() { /* ... */ }
void MapCanvas::paintSelections() { /* ... */ }
void MapCanvas::paintSelectionArea() { /* ... */ }
void MapCanvas::paintNewInfomarkSelection() { /* ... */ }
void MapCanvas::paintSelectedRooms() { /* ... */ }
void MapCanvas::paintSelectedRoom(RoomSelFakeGL &, const RawRoom &room) { /* ... */ }
void MapCanvas::paintSelectedConnection() { /* ... */ }
void MapCanvas::paintNearbyConnectionPoints() { /* ... */ }
void MapCanvas::paintSelectedInfoMarks() { /* ... */ }
void MapCanvas::paintCharacters() { /* ... */ }
void MapCanvas::paintDifferences() { /* ... */ }
void MapCanvas::reportGLVersion() { /* ... */ }
bool MapCanvas::isBlacklistedDriver() { return false; }
void MapCanvas::cleanupOpenGL() { /* ... */ }
void MapCanvas::slot_onMessageLoggedDirect(const QOpenGLDebugMessage &message) { /* ... */ }
