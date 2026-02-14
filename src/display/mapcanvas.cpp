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
#include "MapCanvasConfig.h"
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
#include <QtGui>

#if defined(_MSC_VER) || defined(__MINGW32__)
#undef near // Bad dog, Microsoft; bad dog!!!
#undef far  // Bad dog, Microsoft; bad dog!!!
#endif

namespace {
constexpr float GESTURE_EPSILON = 1e-6f;
constexpr float PINCH_DISTANCE_THRESHOLD = 1e-3f;
} // namespace

using NonOwningPointer = MapCanvas *;
NODISCARD static NonOwningPointer &primaryMapCanvas()
{
    static NonOwningPointer primary = nullptr;
    return primary;
}

MapCanvas::MapCanvas(MapData &mapData,
                     PrespammedPath &prespammedPath,
                     Mmapper2Group &groupManager,
                     QWindow *const parent)
    : QOpenGLWindow{NoPartialUpdate, parent}
    , MapCanvasViewport{static_cast<QWindow &>(*this)}
    , MapCanvasInputState{mapData, prespammedPath}
    , m_opengl{}
    , m_glFont{m_opengl}
    , m_data{mapData}
    , m_groupManager{groupManager}
{
    NonOwningPointer &pmc = primaryMapCanvas();
    if (pmc == nullptr) {
        pmc = this;
    }

    setCursor(Qt::OpenHandCursor);
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
    MapCanvasViewport::layerUp();
    layerChanged();
}

void MapCanvas::slot_layerDown()
{
    MapCanvasViewport::layerDown();
    layerChanged();
}

void MapCanvas::slot_layerReset()
{
    MapCanvasViewport::layerReset();
    layerChanged();
}

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

    // Let the MainWindow know
    emit sig_newRoomSelection(selection);
    selectionChanged();
}

void MapCanvas::slot_setConnectionSelection(const std::shared_ptr<ConnectionSelection> &selection)
{
    m_connectionSelection = selection;
    emit sig_newConnectionSelection(selection.get());
    selectionChanged();
}

void MapCanvas::slot_setInfomarkSelection(const std::shared_ptr<InfomarkSelection> &selection)
{
    if (m_canvasMouseMode == CanvasMouseModeEnum::CREATE_INFOMARKS) {
        m_infoMarkSelection = selection;

    } else if (selection == nullptr || selection->empty()) {
        m_infoMarkSelection = nullptr;

    } else {
        qDebug() << "Updated selection with" << selection->size() << "infomarks";
        m_infoMarkSelection = selection;
    }

    emit sig_newInfomarkSelection(m_infoMarkSelection.get());
    selectionChanged();
}

void MapCanvas::wheelEvent(QWheelEvent *const event)
{
    updateModifierState(event);

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
        if (m_ctrlPressed) {
            if (event->angleDelta().y() > 100) {
                slot_layerDown();
            } else if (event->angleDelta().y() < -100) {
                slot_layerUp();
            }
        } else {
            // Change the zoom level
            const int angleDelta = event->angleDelta().y();
            if (angleDelta != 0) {
                const float factor = std::pow(ScaleFactor::ZOOM_STEP,
                                              static_cast<float>(angleDelta) / 120.0f);
                if (const auto xy = getMouseCoords(event)) {
                    zoomAt(factor, *xy);
                }
            }
        }
        break;

    case CanvasMouseModeEnum::NONE:
    default:
        break;
    }

    event->accept();
}

void MapCanvas::slot_onForcedPositionChange()
{
    slot_requestUpdate();
}

void MapCanvas::touchEvent(QTouchEvent *const event)
{
    if (event->type() == QEvent::TouchBegin) {
        emit sig_dismissContextMenu();
    }

    if (const auto deltaFactor = calculatePinchDelta(event)) {
        handleZoomAtEvent(event, *deltaFactor);
        event->accept();
    } else {
        if (event->points().size() > 2) {
            // Explicitly ignore more than 2 touch points for pinch zoom.
            qDebug() << "MapCanvas::touchEvent: ignoring" << event->points().size()
                     << "touch points";
        }
        QOpenGLWindow::touchEvent(event);
    }
}

void MapCanvas::handleZoomAtEvent(const QInputEvent *const event, const float deltaFactor)
{
    if (std::abs(deltaFactor - 1.f) > GESTURE_EPSILON) {
        if (const auto xy = getMouseCoords(event)) {
            zoomAt(deltaFactor, *xy);
        }
    }
}

bool MapCanvas::event(QEvent *const event)
{
    if (event->type() == QEvent::NativeGesture) {
        auto *const nativeEvent = static_cast<QNativeGestureEvent *>(event);
        if (nativeEvent->gestureType() == Qt::ZoomNativeGesture) {
            if (const auto deltaFactor = calculateNativeZoomDelta(nativeEvent)) {
                handleZoomAtEvent(nativeEvent, *deltaFactor);
            }
            event->accept();
            return true;
        }
    }

    return QOpenGLWindow::event(event);
}

void MapCanvas::slot_createRoom()
{
    if (!hasSel1()) {
        return;
    }

    const Coordinate c = getSel1().getCoordinate();
    if (const auto &existing = m_data.findRoomHandle(c)) {
        return;
    }

    if (m_data.createEmptyRoom(Coordinate{c.x, c.y, m_currentLayer})) {
        // success
    } else {
        // failed!
    }
}

// REVISIT: This function doesn't need to return a shared ptr. Consider refactoring InfomarkSelection?
std::shared_ptr<InfomarkSelection> MapCanvas::getInfomarkSelection(const MouseSel &sel)
{
    const auto [lo, hi] = calculateInfomarkProbeRange(sel);
    return InfomarkSelection::alloc(m_data, lo, hi);
}

void MapCanvas::mousePressEvent(QMouseEvent *const event)
{
    if (event->button() != Qt::RightButton) {
        emit sig_dismissContextMenu();
    }

    updateButtonState(event);
    updateModifierState(event);

    if (m_mouseLeftPressed && m_altPressed) {
        m_altDragState.emplace(AltDragState{event->position().toPoint(), cursor()});
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }

    const auto optXy = getMouseCoords(event);
    if (!optXy) {
        return;
    }
    const auto xy = *optXy;
    if (m_canvasMouseMode == CanvasMouseModeEnum::MOVE) {
        const auto worldPos = unproject_clamped(xy);
        m_sel1 = m_sel2 = MouseSel{Coordinate2f{worldPos.x, worldPos.y}, m_currentLayer};
    } else {
        m_sel1 = m_sel2 = getUnprojectedMouseSel(xy);
    }

    if (event->button() == Qt::ForwardButton) {
        slot_layerUp();
        return event->accept();
    } else if (event->button() == Qt::BackButton) {
        slot_layerDown();
        return event->accept();
    } else if (!m_mouseLeftPressed && m_mouseRightPressed) {
        if (m_canvasMouseMode == CanvasMouseModeEnum::MOVE && hasSel1()) {
            handleMoveModeRightClick(getSel1(), static_cast<MapCanvasViewport &>(*this));
            emit sig_newRoomSelection(SigRoomSelection{m_roomSelection});
            emit sig_newInfomarkSelection(m_infoMarkSelection.get());
            selectionChanged();
        }
        emit sig_customContextMenuRequested(event->position().toPoint());
        m_mouseRightPressed = false;
        event->accept();
        return;
    }

    switch (m_canvasMouseMode) {
    case CanvasMouseModeEnum::CREATE_INFOMARKS:
        infomarksChanged();
        break;
    case CanvasMouseModeEnum::SELECT_INFOMARKS:
        // Select infomarks
        if (m_mouseLeftPressed && hasSel1()) {
            auto tmpSel = getInfomarkSelection(getSel1());
            if (m_infoMarkSelection != nullptr && !tmpSel->empty()
                && m_infoMarkSelection->contains(tmpSel->front().getId())) {
                m_infoMarkSelectionMove.reset();   // dtor, if necessary
                m_infoMarkSelectionMove.emplace(); // ctor
            } else {
                m_selectedArea = false;
                m_infoMarkSelectionMove.reset();
            }
        }
        selectionChanged();
        break;
    case CanvasMouseModeEnum::MOVE:
        if (m_mouseLeftPressed && hasSel1()) {
            setCursor(Qt::ClosedHandCursor);
            startMoving(m_sel1.value());
        }
        break;

    case CanvasMouseModeEnum::RAYPICK_ROOMS:
        if (m_mouseLeftPressed) {
            RoomIdSet tmpSel;
            for (const auto &c2 : calculateRaypickCoordinates(xy)) {
                if (const auto r = m_data.findRoomHandle(c2)) {
                    tmpSel.insert(r.getId());
                }
            }

            if (!tmpSel.empty()) {
                slot_setRoomSelection(
                    SigRoomSelection(RoomSelection::createSelection(std::move(tmpSel))));
            }

            selectionChanged();
        }
        break;

    case CanvasMouseModeEnum::SELECT_ROOMS:
        // Cancel
        if (m_mouseRightPressed) {
            m_selectedArea = false;
            slot_clearRoomSelection();
        }
        // Select rooms
        if (m_mouseLeftPressed && hasSel1()) {
            if (!m_ctrlPressed) {
                const auto pRoom = m_data.findRoomHandle(getSel1().getCoordinate());
                if (pRoom.exists() && m_roomSelection != nullptr
                    && m_roomSelection->contains(pRoom.getId())) {
                    m_roomSelectionMove.emplace(RoomSelMove{});
                } else {
                    m_roomSelectionMove.reset();
                    m_selectedArea = false;
                    slot_clearRoomSelection();
                }
            } else {
                m_ctrlPressed = true;
            }
        }
        selectionChanged();
        break;

    case CanvasMouseModeEnum::CREATE_ONEWAY_CONNECTIONS:
    case CanvasMouseModeEnum::CREATE_CONNECTIONS:
        // Select connection
        if (m_mouseLeftPressed && hasSel1()) {
            m_connectionSelection = ConnectionSelection::alloc(m_data, getSel1());
            if (!m_connectionSelection->isFirstValid()) {
                m_connectionSelection = nullptr;
            }
            emit sig_newConnectionSelection(nullptr);
        }
        // Cancel
        if (m_mouseRightPressed) {
            slot_clearConnectionSelection();
        }
        selectionChanged();
        break;

    case CanvasMouseModeEnum::SELECT_CONNECTIONS:
        if (m_mouseLeftPressed && hasSel1()) {
            m_connectionSelection = ConnectionSelection::alloc(m_data, getSel1());
            if (!m_connectionSelection->isFirstValid()) {
                m_connectionSelection = nullptr;
            } else {
                const auto &r1 = m_connectionSelection->getFirst().room;
                const ExitDirEnum dir1 = m_connectionSelection->getFirst().direction;

                if (r1.getExit(dir1).outIsEmpty()) {
                    m_connectionSelection = nullptr;
                }
            }
            emit sig_newConnectionSelection(nullptr);
        }
        // Cancel
        if (m_mouseRightPressed) {
            slot_clearConnectionSelection();
        }
        selectionChanged();
        break;

    case CanvasMouseModeEnum::CREATE_ROOMS:
        slot_createRoom();
        break;

    case CanvasMouseModeEnum::NONE:
    default:
        break;
    }
}

void MapCanvas::mouseMoveEvent(QMouseEvent *const event)
{
    const auto optXy = getMouseCoords(event);
    if (!optXy) {
        return;
    }
    const auto xy = *optXy;

    if (m_altDragState.has_value()) {
        // The user released the Alt key mid-drag.
        if (!((event->modifiers() & Qt::AltModifier) != 0u)) {
            setCursor(m_altDragState->originalCursor);
            m_altDragState.reset();
            // Don't accept the event; let the underlying widgets handle it.
            return;
        }

        auto &dragState = m_altDragState.value();
        const auto pos = event->position().toPoint();
        const auto delta = pos - dragState.lastPos;
        dragState.lastPos = pos;

        if (applyRotationDelta(delta.x(), delta.y())) {
            graphicsSettingsChanged();
        }
        event->accept();
        return;
    }

    updateButtonState(event);

    if (m_canvasMouseMode != CanvasMouseModeEnum::MOVE) {
        const auto [hScroll, vScroll] = calculateContinuousScroll(xy);
        emit sig_continuousScroll(hScroll, vScroll);
    }

    m_sel2 = getUnprojectedMouseSel(xy);

    switch (m_canvasMouseMode) {
    case CanvasMouseModeEnum::SELECT_INFOMARKS:
        if (m_mouseLeftPressed && hasSel1() && hasSel2()) {
            if (hasInfomarkSelectionMove()) {
                m_infoMarkSelectionMove->pos = getSel2().pos - getSel1().pos;
                setCursor(Qt::ClosedHandCursor);

            } else {
                m_selectedArea = true;
            }
        }
        selectionChanged();
        break;
    case CanvasMouseModeEnum::CREATE_INFOMARKS:
        if (m_mouseLeftPressed) {
            m_selectedArea = true;
        }
        // REVISIT: It doesn't look like anything actually changed here?
        infomarksChanged();
        break;
    case CanvasMouseModeEnum::MOVE:
        if (m_mouseLeftPressed && m_dragState.has_value()) {
            if (performPanning(xy,
                               m_dragState->startWorldPos,
                               m_dragState->startScroll,
                               m_dragState->startViewProj)) {
                emit sig_onCenter(m_scroll);
                update();
            }
        }
        break;

    case CanvasMouseModeEnum::RAYPICK_ROOMS:
        break;

    case CanvasMouseModeEnum::SELECT_ROOMS:
        if (m_mouseLeftPressed) {
            if (hasRoomSelectionMove() && hasSel1() && hasSel2()) {
                auto isMovable = [this](RoomSelection &sel, const Coordinate &offset) -> bool {
                    sel.removeMissing(m_mapData);
                    return m_mapData.getCurrentMap().wouldAllowRelativeMove(sel.getRoomIds(),
                                                                            offset);
                };

                const auto diff = getSel2().pos.truncate() - getSel1().pos.truncate();
                const auto wrongPlace = !isMovable(deref(m_roomSelection), Coordinate{diff, 0});
                m_roomSelectionMove->pos = diff;
                m_roomSelectionMove->wrongPlace = wrongPlace;

                setCursor(wrongPlace ? Qt::ForbiddenCursor : Qt::ClosedHandCursor);
            } else {
                m_selectedArea = true;
            }
        }
        selectionChanged();
        break;

    case CanvasMouseModeEnum::CREATE_ONEWAY_CONNECTIONS:
    case CanvasMouseModeEnum::CREATE_CONNECTIONS:
        if (m_mouseLeftPressed && hasSel2()) {
            if (m_connectionSelection != nullptr) {
                m_connectionSelection->setSecond(getSel2());

                if (m_connectionSelection->isValid() && !m_connectionSelection->isCompleteNew()) {
                    m_connectionSelection->removeSecond();
                }

                selectionChanged();
            }
        } else {
            slot_requestUpdate();
        }
        break;

    case CanvasMouseModeEnum::SELECT_CONNECTIONS:
        if (m_mouseLeftPressed && hasSel2()) {
            if (m_connectionSelection != nullptr) {
                m_connectionSelection->setSecond(getSel2());

                if (!m_connectionSelection->isCompleteExisting()) {
                    m_connectionSelection->removeSecond();
                }

                selectionChanged();
            }
        }
        break;

    case CanvasMouseModeEnum::CREATE_ROOMS:
    case CanvasMouseModeEnum::NONE:
    default:
        break;
    }
}

void MapCanvas::mouseReleaseEvent(QMouseEvent *const event)
{
    const auto optXy = getMouseCoords(event);
    if (!optXy) {
        return;
    }
    const auto xy = *optXy;

    if (m_altDragState.has_value()) {
        setCursor(m_altDragState->originalCursor);
        m_altDragState.reset();
        event->accept();
        return;
    }

    emit sig_continuousScroll(0, 0);
    m_sel2 = getUnprojectedMouseSel(xy);

    if (m_mouseRightPressed) {
        m_mouseRightPressed = false;
    }

    switch (m_canvasMouseMode) {
    case CanvasMouseModeEnum::SELECT_INFOMARKS:
        setCursor(Qt::ArrowCursor);
        if (m_mouseLeftPressed) {
            m_mouseLeftPressed = false;
            const bool wasMoving = hasInfomarkSelectionMove();
            if (handleInfomarkSelectionRelease()) {
                infomarksChanged();
            } else if (!wasMoving && m_infoMarkSelection && m_infoMarkSelection->size() == 1) {
                const InfomarkHandle &firstMark = m_infoMarkSelection->front();
                const Coordinate &pos = firstMark.getPosition1();
                QString ctemp = QString("Selected Info Mark: [%1] (at %2,%3,%4)")
                                    .arg(firstMark.getText().toQString())
                                    .arg(pos.x)
                                    .arg(pos.y)
                                    .arg(pos.z);
                log(ctemp);
            }
            emit sig_newInfomarkSelection(m_infoMarkSelection.get());
        }
        selectionChanged();
        break;

    case CanvasMouseModeEnum::CREATE_INFOMARKS:
        if (m_mouseLeftPressed && hasSel1() && hasSel2()) {
            m_mouseLeftPressed = false;
            (void) handleInfomarkSelectionRelease();
            emit sig_newInfomarkSelection(m_infoMarkSelection.get());
        }
        infomarksChanged();
        break;

    case CanvasMouseModeEnum::MOVE:
        stopMoving();
        setCursor(Qt::OpenHandCursor);
        if (m_mouseLeftPressed) {
            m_mouseLeftPressed = false;
        }
        // Display a room info tooltip if there was no mouse movement
        if (event->button() == Qt::LeftButton && hasSel1() && hasSel2()
            && getSel1().to_vec3() == getSel2().to_vec3()) {
            if (const auto room = m_data.findRoomHandle(getSel1().getCoordinate())) {
                // Tooltip doesn't support ANSI, and there's no way to add formatted text.
                auto message = mmqt::previewRoom(room,
                                                 mmqt::StripAnsiEnum::Yes,
                                                 mmqt::PreviewStyleEnum::ForDisplay);

                const auto pos = event->position().toPoint();
                emit sig_showTooltip(message, pos);
            }
        }
        break;

    case CanvasMouseModeEnum::RAYPICK_ROOMS:
        break;

    case CanvasMouseModeEnum::SELECT_ROOMS:
        setCursor(Qt::ArrowCursor);

        // This seems very unusual.
        if (m_ctrlPressed && m_altPressed) {
            break;
        }

        if (m_mouseLeftPressed) {
            m_mouseLeftPressed = false;
            handleRoomSelectionRelease();
            if (m_roomSelection != nullptr && !m_roomSelection->empty()) {
                slot_setRoomSelection(SigRoomSelection{m_roomSelection});
            }
        }
        selectionChanged();
        break;

    case CanvasMouseModeEnum::CREATE_ONEWAY_CONNECTIONS:
    case CanvasMouseModeEnum::CREATE_CONNECTIONS:
        if (m_mouseLeftPressed && hasSel1() && hasSel2()) {
            m_mouseLeftPressed = false;

            if (m_connectionSelection == nullptr) {
                m_connectionSelection = ConnectionSelection::alloc(m_data, getSel1());
            }
            m_connectionSelection->setSecond(getSel2());

            if (!m_connectionSelection->isValid()) {
                m_connectionSelection = nullptr;
            } else {
                const auto first = m_connectionSelection->getFirst();
                const auto second = m_connectionSelection->getSecond();
                const auto &r1 = first.room;
                const auto &r2 = second.room;

                if (r1.exists() && r2.exists()) {
                    const ExitDirEnum dir1 = first.direction;
                    const ExitDirEnum dir2 = second.direction;

                    const RoomId id1 = r1.getId();
                    const RoomId id2 = r2.getId();

                    const bool isCompleteNew = m_connectionSelection->isCompleteNew();
                    m_connectionSelection = nullptr;

                    if (isCompleteNew) {
                        const WaysEnum ways = (m_canvasMouseMode
                                               != CanvasMouseModeEnum::CREATE_ONEWAY_CONNECTIONS)
                                                  ? WaysEnum::TwoWay
                                                  : WaysEnum::OneWay;
                        const exit_change_types::ModifyExitConnection change{ChangeTypeEnum::Add,
                                                                             id1,
                                                                             dir1,
                                                                             id2,
                                                                             ways};
                        m_data.applySingleChange(Change{change});
                    }
                    m_connectionSelection = ConnectionSelection::alloc(m_data);
                    m_connectionSelection->setFirst(id1, dir1);
                    m_connectionSelection->setSecond(id2, dir2);
                    slot_mapChanged();
                }
            }

            slot_setConnectionSelection(m_connectionSelection);
        }
        selectionChanged();
        break;

    case CanvasMouseModeEnum::SELECT_CONNECTIONS:
        if (m_mouseLeftPressed && (m_connectionSelection != nullptr || hasSel1()) && hasSel2()) {
            m_mouseLeftPressed = false;

            if (m_connectionSelection == nullptr && hasSel1()) {
                m_connectionSelection = ConnectionSelection::alloc(m_data, getSel1());
            }
            m_connectionSelection->setSecond(getSel2());

            if (!m_connectionSelection->isValid() || !m_connectionSelection->isCompleteExisting()) {
                m_connectionSelection = nullptr;
            }
            slot_setConnectionSelection(m_connectionSelection);
        }
        selectionChanged();
        break;

    case CanvasMouseModeEnum::CREATE_ROOMS:
        if (m_mouseLeftPressed) {
            m_mouseLeftPressed = false;
        }
        break;

    case CanvasMouseModeEnum::NONE:
    default:
        break;
    }

    m_altPressed = false;
    m_ctrlPressed = false;
}

void MapCanvas::startMoving(const MouseSel &startPos)
{
    MapCanvasInputState::startMoving(startPos, m_scroll, m_viewProj);
}

void MapCanvas::stopMoving()
{
    MapCanvasInputState::stopMoving();
}

void MapCanvas::zoomAt(const float factor, const glm::vec2 &mousePos)
{
    const bool want3D = MapCanvasConfig::isIn3dMode();
    MapCanvasViewport::zoomAt(factor, mousePos, want3D);
    zoomChanged();
    emit sig_onCenter(m_scroll);
    update();
}

void MapCanvas::slot_setScroll(const glm::vec2 &worldPos)
{
    m_scroll = worldPos;
    update();
}

void MapCanvas::slot_setHorizontalScroll(const float worldX)
{
    m_scroll.x = worldX;
    update();
}

void MapCanvas::slot_setVerticalScroll(const float worldY)
{
    m_scroll.y = worldY;
    update();
}

void MapCanvas::slot_zoomIn()
{
    MapCanvasViewport::zoomIn();
    zoomChanged();
    update();
}

void MapCanvas::slot_zoomOut()
{
    MapCanvasViewport::zoomOut();
    zoomChanged();
    update();
}

void MapCanvas::slot_zoomReset()
{
    MapCanvasViewport::zoomReset();
    zoomChanged();
    update();
}

void MapCanvas::onMovement()
{
    const Coordinate &pos = m_data.tryGetPosition().value_or(Coordinate{});
    MapCanvasViewport::centerOn(pos);
    emit sig_onCenter(m_scroll);
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

void MapCanvas::slot_requestUpdate()
{
    update();
}

void MapCanvas::screenChanged()
{
    auto &gl = getOpenGL();
    if (!gl.isRendererInitialized()) {
        return;
    }

    const auto newDpi = static_cast<float>(QPaintDevice::devicePixelRatioF());
    const auto oldDpi = gl.getDevicePixelRatio();

    if (!utils::equals(newDpi, oldDpi)) {
        log(QString("Display: %1 DPI").arg(static_cast<double>(newDpi)));

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
    handleEscape();
    selectionChanged();
}
