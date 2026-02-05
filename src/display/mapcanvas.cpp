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
#include "../mapdata/mapdata.h"
#include "../mapdata/roomselection.h"
#include "InfomarkSelection.h"
#include "MapCanvasData.h"
#include "MapCanvasRoomDrawer.h"
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
#include <QToolTip>
#include <QtGui>
#include <QtWidgets>

#if defined(_MSC_VER) || defined(__MINGW32__)
#undef near // Bad dog, Microsoft; bad dog!!!
#undef far  // Bad dog, Microsoft; bad dog!!!
#endif

using NonOwningPointer = MapCanvas *;
NODISCARD static NonOwningPointer &primaryMapCanvas()
{
    static NonOwningPointer primary = nullptr;
    return primary;
}

MapCanvas::MapCanvas(MapData &mapData,
                     PrespammedPath &prespammedPath,
                     Mmapper2Group &groupManager,
                     QWidget *const parent)
    : QOpenGLWidget{parent}
    , m_viewModel(std::make_unique<MapCanvasViewModel>(mapData, prespammedPath))
    , m_mapScreen{*m_viewModel}
    , m_opengl{}
    , m_glFont{m_opengl}
    , m_data{mapData}
    , m_groupManager{groupManager}
{
    NonOwningPointer &pmc = primaryMapCanvas();
    if (pmc == nullptr) { pmc = this; }
    setCursor(Qt::OpenHandCursor); grabGesture(Qt::PinchGesture); setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_viewModel.get(), &MapCanvasViewModel::sig_requestUpdate, this, &MapCanvas::slot_requestUpdate);
    connect(m_viewModel.get(), &MapCanvasViewModel::sig_mapMove, this, &MapCanvas::sig_mapMove);
    connect(m_viewModel.get(), &MapCanvasViewModel::sig_setCursor, this, &MapCanvas::setCursor);
    connect(m_viewModel.get(), &MapCanvasViewModel::zoomChanged, this, [this]() { emit sig_zoomChanged(m_viewModel->zoom()); });
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

void MapCanvas::slot_layerUp() { m_viewModel->layerUp(); }

void MapCanvas::slot_layerDown() { m_viewModel->layerDown(); }

void MapCanvas::slot_layerReset() { m_viewModel->layerReset(); }

void MapCanvas::slot_setCanvasMouseMode(const CanvasMouseModeEnum mode)
{
    // FIXME: This probably isn't what you actually want here,
    // since it clears selections when re-choosing the same mode,
    // or when changing to a compatible mode.
    //
    // e.g. RAYPICK_ROOMS vs SELECT_CONNECTIONS,
    // or if you're trying to use MOVE to pan to reach more rooms
    // (granted, the latter isn't "necessary" because you can use
    // scrollbars or use the new zoom-recenter feature).
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

    m_viewModel->m_canvasMouseMode = mode;
    m_viewModel->m_selectedArea = false;
    selectionChanged();
}

void MapCanvas::slot_setRoomSelection(const SigRoomSelection &selection)
{
    if (!selection.isValid()) {
        m_viewModel->m_roomSelection.reset();
    } else {
        m_viewModel->m_roomSelection = selection.getShared();
        auto &sel = deref(m_viewModel->m_roomSelection);
        auto &map = m_data;
        sel.removeMissing(map);

        qDebug() << "Updated selection with" << sel.size() << "rooms";
        if (sel.size() == 1) {
            if (const auto r = map.findRoomHandle(sel.getFirstRoomId())) {
                auto message = mmqt::previewRoom(r,
                                                 mmqt::StripAnsiEnum::Yes,
                                                 mmqt::PreviewStyleEnum::ForLog);
                mapLog(message);
            }
        }
    }

    // Let the MainWindow know
    emit sig_newRoomSelection(selection);
    selectionChanged();
}

void MapCanvas::slot_setConnectionSelection(const std::shared_ptr<ConnectionSelection> &selection)
{
    m_viewModel->m_connectionSelection = selection;
    emit sig_newConnectionSelection(selection.get());
    selectionChanged();
}

void MapCanvas::slot_setInfomarkSelection(const std::shared_ptr<InfomarkSelection> &selection)
{
    if (m_viewModel->m_canvasMouseMode == CanvasMouseModeEnum::CREATE_INFOMARKS) {
        m_viewModel->m_infoMarkSelection = selection;

    } else if (selection == nullptr || selection->empty()) {
        m_viewModel->m_infoMarkSelection = nullptr;

    } else {
        qDebug() << "Updated selection with" << selection->size() << "infomarks";
        m_viewModel->m_infoMarkSelection = selection;
    }

    emit sig_newInfomarkSelection(m_viewModel->m_infoMarkSelection.get());
    selectionChanged();
}

NODISCARD static uint32_t operator&(const Qt::KeyboardModifiers left, const Qt::Modifier right)
{
    return static_cast<uint32_t>(left) & static_cast<uint32_t>(right);
}

void MapCanvas::wheelEvent(QWheelEvent *const event) { m_viewModel->handleWheel(event); }



bool MapCanvas::event(QEvent *const event) { return m_viewModel->handleEvent(event) || QOpenGLWidget::event(event); }

void MapCanvas::slot_createRoom()
{
    if (!m_viewModel->hasSel1()) {
        return;
    }

    const Coordinate c = m_viewModel->getSel1().getCoordinate();
    if (const auto &existing = m_data.findRoomHandle(c)) {
        return;
    }

    if (m_data.createEmptyRoom(Coordinate{c.x, c.y, m_viewModel->m_currentLayer})) {
        // success
    } else {
        // failed!
    }
}

// REVISIT: This function doesn't need to return a shared ptr. Consider refactoring InfomarkSelection?
std::shared_ptr<InfomarkSelection> MapCanvas::getInfomarkSelection(const MouseSel &sel)
{
    static constexpr float CLICK_RADIUS = 10.f;

    const auto center = sel.to_vec3();
    const auto optClickPoint = m_viewModel->project(center);
    if (!optClickPoint) {
        // This should never happen, so we'll crash in debug, but let's do something
        // "sane" in release build since the assert will not trigger.
        assert(false);

        // This distance is in world space, but the mouse click is in screen space.
        static_assert(INFOMARK_SCALE % 5 == 0);
        static constexpr auto INFOMARK_CLICK_RADIUS = INFOMARK_SCALE / 5;
        const auto pos = sel.getScaledCoordinate(INFOMARK_SCALE);
        const auto lo = pos + Coordinate{-INFOMARK_CLICK_RADIUS, -INFOMARK_CLICK_RADIUS, 0};
        const auto hi = pos + Coordinate{+INFOMARK_CLICK_RADIUS, +INFOMARK_CLICK_RADIUS, 0};
        return InfomarkSelection::alloc(m_data, lo, hi);
    }

    const glm::vec2 clickPoint = glm::vec2{optClickPoint.value()};
    glm::vec3 maxCoord = center;
    glm::vec3 minCoord = center;

    const auto probe = [this, &clickPoint, &maxCoord, &minCoord](const glm::vec2 &offset) -> void {
        const auto coord = m_viewModel->unproject_clamped(clickPoint + offset);
        maxCoord = glm::max(maxCoord, coord);
        minCoord = glm::min(minCoord, coord);
    };

    // screen space can rotate relative to world space, and the projected world
    // space positions can be highly ansitropic (e.g. steep vertical angle),
    // so we'll expand the search area by probing in all 45 and 90 degree angles
    // from the mouse. This isn't perfect, but it should be good enough.
    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            probe(glm::vec2{static_cast<float>(dx) * CLICK_RADIUS,
                            static_cast<float>(dy) * CLICK_RADIUS});
        }
    }

    const auto getScaled = [this](const glm::vec3 &c) -> Coordinate {
        const auto pos = glm::ivec3(glm::vec2(c) * static_cast<float>(INFOMARK_SCALE),
                                    m_viewModel->m_currentLayer);
        return Coordinate{pos.x, pos.y, pos.z};
    };

    const auto hi = getScaled(maxCoord);
    const auto lo = getScaled(minCoord);

    return InfomarkSelection::alloc(m_data, lo, hi);
}

void MapCanvas::mousePressEvent(QMouseEvent *const event) { m_viewModel->handleMousePress(event); }

void MapCanvas::mouseMoveEvent(QMouseEvent *const event) { m_viewModel->handleMouseMove(event); }

void MapCanvas::mouseReleaseEvent(QMouseEvent *const event) { m_viewModel->handleMouseRelease(event); }

QSize MapCanvas::minimumSizeHint() const
{
    return {sizeHint().width() / 4, sizeHint().height() / 4};
}

QSize MapCanvas::sizeHint() const
{
    return {1280, 720};
}

void MapCanvas::slot_setScroll(const glm::vec2 &worldPos)
{
    m_viewModel->m_scroll = worldPos;
    update();
}

void MapCanvas::slot_setHorizontalScroll(const float worldX)
{
    m_viewModel->m_scroll.x = worldX;
    update();
}

void MapCanvas::slot_setVerticalScroll(const float worldY)
{
    m_viewModel->m_scroll.y = worldY;
    update();
}

void MapCanvas::slot_zoomIn() { m_viewModel->zoomIn(); }

void MapCanvas::slot_zoomOut() { m_viewModel->zoomOut(); }

void MapCanvas::slot_zoomReset() { m_viewModel->zoomReset(); }

void MapCanvas::onMovement()
{
    const Coordinate &pos = m_data.tryGetPosition().value_or(Coordinate{});
    m_viewModel->m_currentLayer = pos.z;
    emit sig_onCenter(pos.to_vec2() + glm::vec2{0.5f, 0.5f});
    update();
}

void MapCanvas::slot_dataLoaded()
{
    onMovement();

    // REVISIT: is the makeCurrent necessary for calling update()?
    // MakeCurrentRaii makeCurrentRaii{*this};
    forceUpdateMeshes();
}

void MapCanvas::slot_moveMarker(const RoomId id)
{
    assert(id != INVALID_ROOMID);
    m_data.setRoom(id);
    onMovement();
}

void MapCanvas::infomarksChanged()
{
    m_batches.infomarksMeshes.reset();
    update();
}

void MapCanvas::layerChanged()
{
    update();
}

void MapCanvas::forceUpdateMeshes()
{
    m_batches.resetExistingMeshesAndIgnorePendingRemesh();
    m_diff.resetExistingMeshesAndIgnorePendingRemesh();
    update();
}

void MapCanvas::slot_mapChanged()
{
    // REVISIT: Ideally we'd want to only update the layers/chunks
    // that actually changed.
    if ((false)) {
        m_batches.mapBatches.reset();
    }
    update();
}

void MapCanvas::slot_requestUpdate() { update(); }

void MapCanvas::screenChanged()
{
    auto &gl = getOpenGL();
    if (!gl.isRendererInitialized()) {
        return;
    }

    const auto newDpi = static_cast<float>(QPaintDevice::devicePixelRatioF());
    const auto oldDpi = gl.getDevicePixelRatio();

    if (!utils::equals(newDpi, oldDpi)) {
        mapLog(QString("Display: %1 DPI").arg(static_cast<double>(newDpi)));

        // NOTE: The new in-flight mesh will get UI updates when it "finishes".
        // This means we could save the existing raw mesh data and re-finish it
        // without having to compute it again, right?
        m_batches.resetExistingMeshesButKeepPendingRemesh();

        gl.setDevicePixelRatio(newDpi);
        auto &font = getGLFont();
        font.cleanup();
        font.init();

        update();
    }
}

void MapCanvas::selectionChanged()
{
    update();
    emit sig_selectionChanged();
}

void MapCanvas::graphicsSettingsChanged()
{
    m_opengl.resetNamedColorsBuffer();
    update();
}

void MapCanvas::userPressedEscape(bool /*pressed*/)
{
    // TODO: encapsulate the states so we can easily cancel anything that's in use.

    switch (m_viewModel->m_canvasMouseMode) {
    case CanvasMouseModeEnum::NONE:
    case CanvasMouseModeEnum::CREATE_ROOMS:
        break;

    case CanvasMouseModeEnum::CREATE_CONNECTIONS:
    case CanvasMouseModeEnum::SELECT_CONNECTIONS:
    case CanvasMouseModeEnum::CREATE_ONEWAY_CONNECTIONS:
        slot_clearConnectionSelection(); // calls selectionChanged();
        break;

    case CanvasMouseModeEnum::RAYPICK_ROOMS:
    case CanvasMouseModeEnum::SELECT_ROOMS:
        m_viewModel->m_selectedArea = false;
        m_viewModel->m_roomSelectionMove.reset();
        slot_clearRoomSelection(); // calls selectionChanged();
        break;

    case CanvasMouseModeEnum::MOVE:
        // special case for move: right click selects infomarks
        FALLTHROUGH;
    case CanvasMouseModeEnum::SELECT_INFOMARKS:
    case CanvasMouseModeEnum::CREATE_INFOMARKS:
        m_viewModel->m_infoMarkSelectionMove.reset();
        slot_clearInfomarkSelection(); // calls selectionChanged();
        break;
    }
}

void MapCanvas::setZoom(float zoom) { m_viewModel->setZoom(zoom); }
float MapCanvas::getRawZoom() const { return m_viewModel->zoom(); }
void MapCanvas::slot_clearAllSelections() { m_viewModel->clearAllSelections(); }
void MapCanvas::slot_clearRoomSelection() { m_viewModel->clearRoomSelection(); }
void MapCanvas::slot_clearConnectionSelection() { m_viewModel->clearConnectionSelection(); }
void MapCanvas::slot_clearInfomarkSelection() { m_viewModel->clearInfomarkSelection(); }
void MapCanvas::slot_onForcedPositionChange() { m_viewModel->clearAllSelections(); }

float MapCanvas::getTotalScaleFactor() const { return m_viewModel->getTotalScaleFactor(); }
void MapCanvas::slot_rebuildMeshes() { forceUpdateMeshes(); }
