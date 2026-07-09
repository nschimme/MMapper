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
#include "../observer/gameobserver.h"
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
                     GameObserver &observer,
                     PrespammedPath &prespammedPath,
                     Mmapper2Group &groupManager,
                     QWindow *const parent)
    : QOpenGLWindow{NoPartialUpdate, parent}
    , MapCanvasViewport{static_cast<QWindow &>(*this)}
    , MapCanvasInputState{prespammedPath}
    , m_mapScreen{static_cast<MapCanvasViewport &>(*this)}
    , m_observer{observer}
    , m_opengl{}
    , m_glFont{m_opengl}
    , m_data{mapData}
    , m_groupManager{groupManager}
    , m_frameManager{static_cast<QOpenGLWindow &>(*this), m_opengl.getUboManager()}
    , m_weather{m_opengl, m_data, m_textures, observer, m_frameManager}
{
    syncViewportConfig();

    m_frameManager.registerCallback(m_lifetime, [this]() {
        return m_batches.remeshCookie.isPending() ? FrameManager::AnimationStatusEnum::Continue
                                                  : FrameManager::AnimationStatusEnum::Stop;
    });
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
}

MapCanvas *MapCanvas::getPrimary()
{
    return primaryMapCanvas();
}

void MapCanvas::slot_layerUp()
{
    setCurrentLayer(getCurrentLayer() + 1);
    layerChanged();
}

void MapCanvas::slot_layerDown()
{
    setCurrentLayer(getCurrentLayer() - 1);
    layerChanged();
}

void MapCanvas::slot_layerReset()
{
    setCurrentLayer(0);
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
    m_activeInteraction.reset();
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

NODISCARD static uint32_t operator&(const Qt::KeyboardModifiers left, const Qt::Modifier right)
{
    return static_cast<uint32_t>(left) & static_cast<uint32_t>(right);
}

void MapCanvas::wheelEvent(QWheelEvent *const event)
{
    const bool hasCtrl = (event->modifiers() & Qt::CTRL) != 0u;

    // Handle smooth panning via pixelDelta (trackpad/high-end mouse)
    if (!event->pixelDelta().isNull() && !hasCtrl) {
        const QPoint delta = event->pixelDelta();
        // The delta is in screen pixels, we need to convert it to world displacement.
        // We can approximate this by unprojecting.
        const auto optXy = getMouseCoords(event);
        if (optXy) {
            const auto xy = *optXy;
            const auto a = unproject_clamped(xy);
            const auto b = unproject_clamped(xy - glm::vec2{delta.x(), -delta.y()});
            const glm::vec2 worldDelta = glm::vec2(b - a);
            const glm::vec2 newScroll = getScroll() + worldDelta;
            setScroll(newScroll);
            emit sig_onCenter(newScroll);
            m_frameManager.requestUpdate();
        }
        event->accept();
        return;
    }

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

    const auto &points = event->points();
    if (points.size() == 2) {
        const auto &p1 = points[0];
        const auto &p2 = points[1];

        const glm::vec2 pos1{p1.position().x(), p1.position().y()};
        const glm::vec2 pos2{p2.position().x(), p2.position().y()};
        const glm::vec2 center = (pos1 + pos2) * 0.5f;
        const float dist = glm::distance(pos1, pos2);

        if (event->type() == QEvent::TouchBegin || p1.state() == QEventPoint::Pressed
            || p2.state() == QEventPoint::Pressed) {
            beginPinch(dist);
            // Use unproject_clamped with current viewProj to get world start
            const glm::vec2 screenPos{center.x, static_cast<float>(height()) - center.y};
            const glm::vec3 worldPos = unproject_clamped(screenPos);
            beginPanning(worldPos, getScroll(), getViewProj(), screenPos);
        }

        if (m_pinchState && m_pinchState->initialDistance > PINCH_DISTANCE_THRESHOLD) {
            const float currentPinchFactor = dist / m_pinchState->initialDistance;
            const float deltaFactor = currentPinchFactor / m_pinchState->lastFactor;

            handleZoomAtEvent(event, deltaFactor);
            updatePinch(currentPinchFactor);
        }

        if (m_panningState) {
            const auto &dragState = *m_panningState;
            const glm::vec2 screenCenter = glm::vec2{center.x,
                                                     static_cast<float>(height()) - center.y};
            const glm::vec3 currWorldPos = unproject_clamped(screenCenter, dragState.startViewProj);
            const glm::vec2 delta = glm::vec2(currWorldPos - dragState.startWorldPos);

            if (glm::length(delta) > GESTURE_EPSILON) {
                const glm::vec2 newWorldCenter = dragState.startScroll - delta;
                setScroll(newWorldCenter);
                emit sig_onCenter(newWorldCenter);
                m_frameManager.requestUpdate();
            }
        }

        if (event->type() == QEvent::TouchEnd || p1.state() == QEventPoint::Released
            || p2.state() == QEventPoint::Released) {
            endPinch();
            endPanning();
        }
        event->accept();
    } else {
        if (points.size() > 2) {
            // Explicitly ignore more than 2 touch points for pinch zoom.
            qDebug() << "MapCanvas::touchEvent: ignoring" << points.size() << "touch points";
        }

        endPinch();
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
            const auto value = static_cast<float>(nativeEvent->value());
            float deltaFactor = 1.f;
            if constexpr (CURRENT_PLATFORM == PlatformEnum::Mac) {
                // On macOS, event->value() for ZoomNativeGesture is the magnification delta
                // since the last event.
                deltaFactor += value;
            } else {
                // On other platforms, it's typically the cumulative scale factor (1.0 at start).
                if (nativeEvent->isBeginEvent() || !m_magnificationState) {
                    beginMagnification();
                }

                if (std::abs(m_magnificationState->lastValue) > GESTURE_EPSILON) {
                    deltaFactor = value / m_magnificationState->lastValue;
                }
                updateMagnification(value);

                if (nativeEvent->isEndEvent()) {
                    endMagnification();
                }
            }
            handleZoomAtEvent(nativeEvent, deltaFactor);
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

    if (m_data.createEmptyRoom(Coordinate{c.x, c.y, getCurrentLayer()})) {
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
    const auto optClickPoint = project(center);
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

    const auto probe = [this, &clickPoint, &maxCoord, &minCoord](const glm::vec2 offset) -> void {
        const auto coord = unproject_clamped(clickPoint + offset);
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

    const auto getScaled = [this](const glm::vec3 c) -> Coordinate {
        const auto pos = glm::ivec3(glm::vec2(c) * static_cast<float>(INFOMARK_SCALE),
                                    getCurrentLayer());
        return Coordinate{pos.x, pos.y, pos.z};
    };

    const auto hi = getScaled(maxCoord);
    const auto lo = getScaled(minCoord);

    return InfomarkSelection::alloc(m_data, lo, hi);
}

void MapCanvas::keyPressEvent(QKeyEvent *const event)
{
    if (event->key() == Qt::Key_Space && !event->isAutoRepeat()) {
        m_spacebarPressed = true;
        setCursor(Qt::ClosedHandCursor);
        event->accept();
    }
}

void MapCanvas::keyReleaseEvent(QKeyEvent *const event)
{
    if (event->key() == Qt::Key_Space && !event->isAutoRepeat()) {
        m_spacebarPressed = false;
        // Restore cursor based on mode
        slot_setCanvasMouseMode(m_canvasMouseMode);
        event->accept();
    }
}

void MapCanvas::mousePressEvent(QMouseEvent *const event)
{
    if (event->button() == Qt::RightButton) {
        // We handle RightButton release for context menu now to allow dragging
    } else {
        emit sig_dismissContextMenu();
    }

    if (m_spacebarPressed && event->button() == Qt::LeftButton) {
        // Handled in universal panning
    }

    const bool hasLeftButton = (event->buttons() & Qt::LeftButton) != 0u;
    const bool hasRightButton = (event->buttons() & Qt::RightButton) != 0u;
    const bool hasMiddleButton = (event->buttons() & Qt::MiddleButton) != 0u;
    const bool hasCtrl = (event->modifiers() & Qt::CTRL) != 0u;
    MAYBE_UNUSED const bool hasAlt = (event->modifiers() & Qt::ALT) != 0u;

    if (hasLeftButton && hasAlt) {
        beginAltDrag(event->position().toPoint(), cursor());
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }

    const auto optXy = getMouseCoords(event);
    if (!optXy) {
        return;
    }
    const auto xy = *optXy;

    // Handle universal panning (Space+LMB or MMB)
    if ((hasLeftButton && m_spacebarPressed) || hasMiddleButton) {
        const auto worldPos = unproject_clamped(xy);
        beginPanning(worldPos, getScroll(), MapCanvasViewport::getViewProj(), xy);
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }
    if (m_canvasMouseMode == CanvasMouseModeEnum::MOVE) {
        const auto worldPos = unproject_clamped(xy);
        m_sel1 = m_sel2 = MouseSel{Coordinate2f{worldPos.x, worldPos.y}, getCurrentLayer()};
    } else {
        m_sel1 = m_sel2 = getUnprojectedMouseSel(xy);
    }

    m_mouseLeftPressed = hasLeftButton;
    m_mouseRightPressed = hasRightButton;
    m_mouseMiddlePressed = hasMiddleButton;

    if (event->button() == Qt::ForwardButton) {
        slot_layerUp();
        return event->accept();
    } else if (event->button() == Qt::BackButton) {
        slot_layerDown();
        return event->accept();
    } else if (!m_mouseLeftPressed && m_mouseRightPressed) {
        if (m_canvasMouseMode == CanvasMouseModeEnum::MOVE && hasSel1()) {
            // Select the room under the cursor
            m_roomSelection = RoomSelection::createSelection(
                m_data.findAllRooms(getSel1().getCoordinate()));
            slot_setRoomSelection(SigRoomSelection{m_roomSelection});

            // Select infomarks under the cursor.
            slot_setInfomarkSelection(getInfomarkSelection(getSel1()));

            selectionChanged();
        }
        emit sig_customContextMenuRequested(event->position().toPoint());
        m_mouseRightPressed = false;
        event->accept();
        return;
    }

    switch (m_canvasMouseMode) {
    case CanvasMouseModeEnum::CREATE_INFOMARKS:
    case CanvasMouseModeEnum::SELECT_INFOMARKS:
        if (hasLeftButton && hasSel1()) {
            if (auto *const area = getInteraction<AreaSelectionInteraction>()) {
                // Second click
                const auto c1 = area->anchorPoint.getScaledCoordinate(INFOMARK_SCALE);
                const auto c2 = getSel1().getScaledCoordinate(INFOMARK_SCALE);

                if (m_canvasMouseMode == CanvasMouseModeEnum::SELECT_INFOMARKS) {
                    auto tmpSel = InfomarkSelection::alloc(m_data, c1, c2);
                    slot_setInfomarkSelection(tmpSel);
                } else {
                    auto tmpSel = InfomarkSelection::allocEmpty(m_data, c1, c2);
                    slot_setInfomarkSelection(tmpSel);
                }
                endInteraction();
            } else {
                // Check if we are moving an existing selection
                auto tmpSel = getInfomarkSelection(getSel1());
                if (m_canvasMouseMode == CanvasMouseModeEnum::SELECT_INFOMARKS
                    && m_infoMarkSelection != nullptr && !tmpSel->empty()
                    && m_infoMarkSelection->contains(tmpSel->front().getId())) {
                    beginInfomarkMove();
                } else {
                    // Start area selection
                    beginAreaSelection(getSel1());
                }
            }
        }
        selectionChanged();
        break;
    case CanvasMouseModeEnum::MOVE:
        if (hasLeftButton && hasSel1()) {
            setCursor(Qt::ClosedHandCursor);
            const auto worldPos = unproject_clamped(xy);
            beginPanning(worldPos, getScroll(), MapCanvasViewport::getViewProj(), xy);
        }
        break;

    case CanvasMouseModeEnum::RAYPICK_ROOMS:
        if (hasLeftButton) {
            const auto near = unproject_raw(glm::vec3{xy, 0.f});
            const auto far = unproject_raw(glm::vec3{xy, 1.f});

            // Note: this rounding assumes we're looking down.
            // We're looking for integer z-planes that cross between
            // the hi and lo points.
            const auto hiz = static_cast<int>(std::floor(near.z));
            const auto loz = static_cast<int>(std::ceil(far.z));
            if (hiz <= loz) {
                break;
            }

            const auto inv_denom = 1.f / (far.z - near.z);

            // REVISIT: This loop might feel laggy.
            // Consider clamping the bounds of what we know we're currently displaying (including XY).
            // (If you're looking almost parallel to the world, then the ray could pass through the
            // visible region but still be within the visible Z-range.)
            RoomIdSet tmpSel;
            for (int z = hiz; z >= loz; --z) {
                const float t = (static_cast<float>(z) - near.z) * inv_denom;
                assert(isClamped(t, 0.f, 1.f));
                const auto pos = glm::mix(near, far, t);
                assert(static_cast<int>(std::lround(pos.z)) == z);
                const Coordinate c2 = MouseSel{Coordinate2f{pos.x, pos.y}, z}.getCoordinate();
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
        if (hasRightButton) {
            endInteraction();
            slot_clearRoomSelection();
        }
        // Select rooms
        if (hasLeftButton && hasSel1()) {
            if (auto *const area = getInteraction<AreaSelectionInteraction>()) {
                // Second click
                const Coordinate c1 = area->anchorPoint.getCoordinate();
                const Coordinate c2 = getSel1().getCoordinate();
                if (m_roomSelection == nullptr || !hasCtrl) {
                    m_roomSelection = RoomSelection::createSelection(m_data.findAllRooms(c1, c2));
                } else {
                    auto &sel = deref(m_roomSelection);
                    sel.removeMissing(m_data);
                    const auto tmpSel = m_data.findAllRooms(c1, c2);
                    for (const RoomId key : tmpSel) {
                        if (sel.contains(key)) {
                            sel.erase(key);
                        } else {
                            sel.insert(key);
                        }
                    }
                }
                if (m_roomSelection != nullptr && !m_roomSelection->empty()) {
                    slot_setRoomSelection(SigRoomSelection{m_roomSelection});
                }
                endInteraction();
            } else {
                if (!hasCtrl) {
                    const auto pRoom = m_data.findRoomHandle(getSel1().getCoordinate());
                    if (pRoom.exists() && m_roomSelection != nullptr
                        && m_roomSelection->contains(pRoom.getId())) {
                        beginRoomMove();
                    } else {
                        beginAreaSelection(getSel1());
                    }
                } else {
                    m_ctrlPressed = true;
                    beginAreaSelection(getSel1());
                }
            }
        }
        selectionChanged();
        break;

    case CanvasMouseModeEnum::CREATE_ONEWAY_CONNECTIONS:
    case CanvasMouseModeEnum::CREATE_CONNECTIONS:
        // Select connection
        if (hasLeftButton && hasSel1()) {
            if (auto *const conn = getInteraction<ConnectionInteraction>()) {
                // Second click
                if (m_connectionSelection != nullptr) {
                    m_connectionSelection->setSecond(getSel1());
                    if (m_connectionSelection->isValid() && m_connectionSelection->isCompleteNew()) {
                        const auto first = m_connectionSelection->getFirst();
                        const auto second = m_connectionSelection->getSecond();
                        const auto id1 = first.room.getId();
                        const auto id2 = second.room.getId();
                        const auto dir1 = first.direction;

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
                        slot_mapChanged();

                        // Start new connection from second room, but without a pre-selected direction
                        m_connectionSelection = ConnectionSelection::alloc(m_data);
                        m_connectionSelection->setFirst(id2, ExitDirEnum::NONE);
                    } else {
                        // Invalid second click: leave the first click's anchor in place
                        // instead of discarding the in-progress selection. Right-click cancels.
                        m_connectionSelection->removeSecond();
                    }
                }
            } else {
                // First click: a click that doesn't land on a valid room is a no-op.
                auto candidate = ConnectionSelection::alloc(m_data, getSel1());
                if (candidate->isFirstValid()) {
                    m_connectionSelection = std::move(candidate);
                    beginConnectionInteraction();
                }
            }
            emit sig_newConnectionSelection(m_connectionSelection.get());
        }
        // Cancel
        if (hasRightButton) {
            endInteraction();
            slot_clearConnectionSelection();
        }
        selectionChanged();
        break;

    case CanvasMouseModeEnum::SELECT_CONNECTIONS:
        if (hasLeftButton && hasSel1()) {
            if (auto *const conn = getInteraction<ConnectionInteraction>()) {
                // Second click
                if (m_connectionSelection != nullptr) {
                    m_connectionSelection->setSecond(getSel1());
                    if (!m_connectionSelection->isValid()
                        || !m_connectionSelection->isCompleteExisting()) {
                        // Invalid second click: leave the first click's anchor in place
                        // instead of discarding the in-progress selection. Right-click cancels.
                        m_connectionSelection->removeSecond();
                    } else {
                        endInteraction();
                    }
                }
            } else {
                // First click: try to anchor a new selection here. A click that doesn't
                // land on a valid exit is a no-op, leaving any already-completed
                // selection from a prior two-click gesture in place (right-click to
                // clear it explicitly) instead of silently wiping it out.
                auto candidate = ConnectionSelection::alloc(m_data, getSel1());
                if (candidate->isFirstValid()
                    && !candidate->getFirst()
                            .room.getExit(candidate->getFirst().direction)
                            .outIsEmpty()) {
                    m_connectionSelection = std::move(candidate);
                    beginConnectionInteraction();
                }
            }
            emit sig_newConnectionSelection(m_connectionSelection.get());
        }
        // Cancel
        if (hasRightButton) {
            endInteraction();
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

    // Handle panning
    if (m_panningState) {
        const auto &dragState = *m_panningState;
        const glm::vec3 currWorldPos = unproject_clamped(xy, dragState.startViewProj);
        const glm::vec2 delta = glm::vec2(currWorldPos - dragState.startWorldPos);

        if (glm::length(delta) > GESTURE_EPSILON) {
            const glm::vec2 newWorldCenter = dragState.startScroll - delta;
            setScroll(newWorldCenter);
            emit sig_onCenter(newWorldCenter);
            m_frameManager.requestUpdate();
        }

        // Keep tool cursor in sync during panning
        m_sel2 = getUnprojectedMouseSel(xy);

        event->accept();
        return;
    }

    if (auto *const altDragState = getInteraction<AltDragState>()) {
        // The user released the Alt key mid-drag.
        if (!((event->modifiers() & Qt::ALT) != 0u)) {
            setCursor(altDragState->originalCursor);
            endInteraction();
            // Don't accept the event; let the underlying widgets handle it.
            return;
        }

        auto &conf = setConfig().canvas.advanced;
        auto &dragState = *altDragState;
        bool angleChanged = false;

        const auto pos = event->position().toPoint();
        const auto delta = pos - dragState.lastPos;
        dragState.lastPos = pos;

        // Camera rotation sensitivity: 3 degree per pixel
        constexpr float SENSITIVITY = 0.3f;

        // Horizontal movement adjusts yaw (horizontalAngle)
        const int dx = delta.x();
        if (dx != 0) {
            conf.horizontalAngle.set(conf.horizontalAngle.get()
                                     + static_cast<int>(static_cast<float>(dx) * SENSITIVITY));
            angleChanged = true;
        }

        // Vertical movement adjusts pitch (verticalAngle), if auto-tilt is off
        if (!conf.autoTilt.get()) {
            const int dy = delta.y();
            if (dy != 0) {
                // Negated to match intuitive up/down dragging
                conf.verticalAngle.set(conf.verticalAngle.get()
                                       + static_cast<int>(static_cast<float>(-dy) * SENSITIVITY));
                angleChanged = true;
            }
        }

        if (angleChanged) {
            graphicsSettingsChanged();
        }
        event->accept();
        return;
    }

    MAYBE_UNUSED const bool hasLeftButton = (event->buttons() & Qt::LeftButton) != 0u;

    m_sel2 = getUnprojectedMouseSel(xy);

    switch (m_canvasMouseMode) {
    case CanvasMouseModeEnum::SELECT_INFOMARKS:
        if (auto *const move = getInteraction<InfomarkSelectionMove>()) {
            move->pos = getSel2().pos - getSel1().pos;
            setCursor(Qt::ClosedHandCursor);
        }
        selectionChanged();
        break;
    case CanvasMouseModeEnum::CREATE_INFOMARKS:
        selectionChanged();
        break;
    case CanvasMouseModeEnum::MOVE:
        break;

    case CanvasMouseModeEnum::RAYPICK_ROOMS:
        break;

    case CanvasMouseModeEnum::SELECT_ROOMS:
        if (auto *const move = getInteraction<RoomSelMove>(); move && hasSel1() && hasSel2()) {
            auto &map = this->m_data;
            auto isMovable = [&map](RoomSelection &sel, const Coordinate offset) -> bool {
                sel.removeMissing(map);
                return map.getCurrentMap().wouldAllowRelativeMove(sel.getRoomIds(), offset);
            };

            const auto diff = getSel2().pos.truncate() - getSel1().pos.truncate();
            const auto wrongPlace = !isMovable(deref(m_roomSelection), Coordinate{diff, 0});
            move->pos = diff;
            move->wrongPlace = wrongPlace;

            setCursor(wrongPlace ? Qt::ForbiddenCursor : Qt::ClosedHandCursor);
        }
        selectionChanged();
        break;

    case CanvasMouseModeEnum::CREATE_ONEWAY_CONNECTIONS:
    case CanvasMouseModeEnum::CREATE_CONNECTIONS:
        // Only mutate the live selection while an interaction is actually in
        // progress (i.e. waiting for the second click). Once a click has
        // completed/ended the interaction, further mouse movement must not
        // silently overwrite the completed second endpoint.
        if (hasConnectionInteraction() && m_connectionSelection != nullptr) {
            m_connectionSelection->setSecond(getSel2());
        }
        // Repaint on every move (not just while an anchor is active) so the
        // nearby-exit hover dots track the mouse before the first click too.
        selectionChanged();
        break;

    case CanvasMouseModeEnum::SELECT_CONNECTIONS:
        if (hasConnectionInteraction() && m_connectionSelection != nullptr) {
            m_connectionSelection->setSecond(getSel2());
        }
        selectionChanged();
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

    if (m_panningState) {
        const bool isUniversalPanning = m_spacebarPressed || m_mouseMiddlePressed;
        const bool isMoveModeLmb = (m_canvasMouseMode == CanvasMouseModeEnum::MOVE
                                    && !isUniversalPanning);

        // A tiny threshold (3 pixels) to differentiate a click from a pan.
        const float dist = glm::distance(xy, m_panningState->startScreenPos);
        const bool isSignificantMovement = (dist > 3.f);

        endPanning();
        // Restore cursor based on mode
        slot_setCanvasMouseMode(m_canvasMouseMode);

        if (!isMoveModeLmb || isSignificantMovement) {
            event->accept();
            return;
        }
        // Fall through for MOVE mode tooltips if movement was minimal
    }

    if (auto *const altDragState = getInteraction<AltDragState>()) {
        setCursor(altDragState->originalCursor);
        endInteraction();
        event->accept();
        return;
    }

    m_sel2 = getUnprojectedMouseSel(xy);

    if (m_mouseRightPressed) {
        m_mouseRightPressed = false;
    }

    switch (m_canvasMouseMode) {
    case CanvasMouseModeEnum::SELECT_INFOMARKS:
        setCursor(Qt::ArrowCursor);
        if (m_mouseLeftPressed) {
            m_mouseLeftPressed = false;
            if (auto *const move = getInteraction<InfomarkSelectionMove>()) {
                const auto pos_copy = move->pos;
                if (m_infoMarkSelection != nullptr) {
                    const auto offset = Coordinate{(pos_copy * INFOMARK_SCALE).truncate(), 0};

                    // Update infomark location
                    const InfomarkSelection &sel = deref(m_infoMarkSelection);
                    sel.applyOffset(offset);
                    infomarksChanged();
                    endInteraction();
                }
            }
        }
        selectionChanged();
        break;

    case CanvasMouseModeEnum::CREATE_INFOMARKS:
        m_mouseLeftPressed = false;
        selectionChanged();
        break;

    case CanvasMouseModeEnum::MOVE:
        setCursor(Qt::OpenHandCursor);
        if (m_mouseLeftPressed) {
            m_mouseLeftPressed = false;
        }
        // Display a room info tooltip if there was no mouse movement
        if (event->button() == Qt::LeftButton && !m_spacebarPressed && hasSel1() && hasSel2()
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
        if (m_mouseLeftPressed) {
            m_mouseLeftPressed = false;
            if (auto *const move = getInteraction<RoomSelMove>()) {
                const auto pos = move->pos;
                const bool wrongPlace = move->wrongPlace;
                if (!wrongPlace && (m_roomSelection != nullptr)) {
                    const Coordinate moverel{pos, 0};
                    m_data.applySingleChange(Change{
                        room_change_types::MoveRelative2{m_roomSelection->getRoomIds(), moverel}});
                }
                endInteraction();
            }
        }
        selectionChanged();
        break;

    case CanvasMouseModeEnum::CREATE_ONEWAY_CONNECTIONS:
    case CanvasMouseModeEnum::CREATE_CONNECTIONS:
        m_mouseLeftPressed = false;
        selectionChanged();
        break;

    case CanvasMouseModeEnum::SELECT_CONNECTIONS:
        m_mouseLeftPressed = false;
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

void MapCanvas::zoomAt(const float factor, const glm::vec2 mousePos)
{
    const auto optWorldPos = unproject(mousePos);
    if (!optWorldPos) {
        ScaleFactor sf = getScaleFactor();
        sf *= factor;
        setScaleFactor(sf);
        zoomChanged();
        m_frameManager.requestUpdate();
        return;
    }

    const glm::vec2 worldPos = glm::vec2(*optWorldPos);

    // Save current state
    const glm::vec2 oldScroll = getScroll();

    // Apply zoom
    ScaleFactor sf = getScaleFactor();
    sf *= factor;
    setScaleFactor(sf);
    zoomChanged();

    // Calculate new scroll position to keep worldPos under mousePos.
    // The unprojection uses the updated zoom level.
    const auto optNewWorldPos = unproject(mousePos);
    if (optNewWorldPos) {
        const glm::vec2 delta = worldPos - glm::vec2(*optNewWorldPos);
        const glm::vec2 newScroll = oldScroll + delta;
        setScroll(newScroll);
        emit sig_onCenter(newScroll);
    } else {
        // Fallback: if we can't find the new world position, just stay where we were.
        setScroll(oldScroll);
        emit sig_onCenter(oldScroll);
    }

    m_frameManager.requestUpdate();
}

void MapCanvas::slot_setScroll(const glm::vec2 worldPos)
{
    if (!utils::isSameFloat(getScroll().x, worldPos.x)
        || !utils::isSameFloat(getScroll().y, worldPos.y)) {
        setScroll(worldPos);
        m_frameManager.requestUpdate();
    }
}

void MapCanvas::slot_setHorizontalScroll(const float worldX)
{
    if (!utils::isSameFloat(getScroll().x, worldX)) {
        auto scroll = getScroll();
        scroll.x = worldX;
        setScroll(scroll);
        m_frameManager.requestUpdate();
    }
}

void MapCanvas::slot_setVerticalScroll(const float worldY)
{
    if (!utils::isSameFloat(getScroll().y, worldY)) {
        auto scroll = getScroll();
        scroll.y = worldY;
        setScroll(scroll);
        m_frameManager.requestUpdate();
    }
}

void MapCanvas::slot_zoomIn()
{
    ScaleFactor sf = getScaleFactor();
    sf.logStep(1);
    setScaleFactor(sf);
    zoomChanged();
    m_frameManager.requestUpdate();
}

void MapCanvas::slot_zoomOut()
{
    ScaleFactor sf = getScaleFactor();
    sf.logStep(-1);
    setScaleFactor(sf);
    zoomChanged();
    m_frameManager.requestUpdate();
}

void MapCanvas::slot_zoomReset()
{
    ScaleFactor sf = getScaleFactor();
    sf.set(1.f);
    setScaleFactor(sf);
    zoomChanged();
    m_frameManager.requestUpdate();
}

void MapCanvas::onMovement()
{
    const Coordinate pos = m_data.tryGetPosition().value_or(Coordinate{});
    setCurrentLayer(pos.z);
    getOpenGL().getUboManager().invalidate(Legacy::SharedVboEnum::CameraBlock);
    const glm::vec2 newScroll = pos.to_vec2() + glm::vec2{0.5f, 0.5f};
    if (!utils::isSameFloat(getScroll().x, newScroll.x)
        || !utils::isSameFloat(getScroll().y, newScroll.y)) {
        setScroll(newScroll);
        emit sig_onCenter(newScroll);
        m_frameManager.requestUpdate();
    }
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
    m_frameManager.requestUpdate();
}

void MapCanvas::layerChanged()
{
    markViewProjDirty();
    m_frameManager.requestUpdate();
}

void MapCanvas::forceUpdateMeshes()
{
    m_batches.resetExistingMeshesAndIgnorePendingRemesh();
    m_diff.resetExistingMeshesAndIgnorePendingRemesh();
    m_frameManager.requestUpdate();
}

void MapCanvas::slot_mapChanged()
{
    // REVISIT: Ideally we'd want to only update the layers/chunks
    // that actually changed.
    if ((false)) {
        m_batches.mapBatches.reset();
    }
    m_frameManager.requestUpdate();
}

void MapCanvas::slot_requestUpdate()
{
    m_frameManager.requestUpdate();
}

void MapCanvas::screenChanged()
{
    auto &gl = getOpenGL();
    if (!gl.isRendererInitialized()) {
        return;
    }

    const auto newDpi = static_cast<float>(QPaintDevice::devicePixelRatioF());
    const auto oldDpi = gl.getDevicePixelRatio();

    if (!utils::isSameFloat(newDpi, oldDpi)) {
        log(QString("Display: %1 DPI").arg(static_cast<double>(newDpi)));

        // NOTE: The new in-flight mesh will get UI updates when it "finishes".
        // This means we could save the existing raw mesh data and re-finish it
        // without having to compute it again, right?
        m_batches.resetExistingMeshesButKeepPendingRemesh();

        gl.setDevicePixelRatio(newDpi);
        auto &font = getGLFont();
        font.cleanup();
        font.init();

        m_frameManager.requestUpdate();
    }
}

void MapCanvas::selectionChanged()
{
    m_frameManager.requestUpdate();
    emit sig_selectionChanged();
}

void MapCanvas::syncViewportConfig()
{
    const auto &advanced = getConfig().canvas.advanced;
    const ViewportConfig config{advanced.use3D.get(),
                                advanced.autoTilt.get(),
                                advanced.fov.getFloat(),
                                advanced.verticalAngle.getFloat(),
                                advanced.horizontalAngle.getFloat(),
                                advanced.layerHeight.getFloat()};
    setViewportConfig(config);
}

void MapCanvas::graphicsSettingsChanged()
{
    m_opengl.resetNamedColorsBuffer();
    syncViewportConfig();
    m_frameManager.requestUpdate();
}

void MapCanvas::userPressedEscape(bool /*pressed*/)
{
    // TODO: encapsulate the states so we can easily cancel anything that's in use.

    switch (m_canvasMouseMode) {
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
        endInteraction();
        slot_clearRoomSelection(); // calls selectionChanged();
        break;

    case CanvasMouseModeEnum::MOVE:
        // special case for move: right click selects infomarks
        FALLTHROUGH;
    case CanvasMouseModeEnum::SELECT_INFOMARKS:
    case CanvasMouseModeEnum::CREATE_INFOMARKS:
        endInteraction();
        slot_clearInfomarkSelection(); // calls selectionChanged();
        break;
    }
}
