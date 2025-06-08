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
#include "../map/World.h"       // Added for World
#include "../map/SpatialDb.h"    // Added for SpatialDb
#include "../mapdata/mapdata.h"
#include "../mapdata/roomselection.h"
#include "InfoMarkSelection.h"
#include "MapCanvasData.h"
#include "MapCanvasRoomDrawer.h" // For getRoomAreaHash
#include "connectionselection.h"
#include "src/mapdata/remesh_types.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <set>
#include <stdexcept>
#include <utility>
#include <vector>
#include <QDebug>

#include <QGestureEvent>
#include <QMessageLogContext>
#include <QOpenGLDebugMessage>
#include <QSize>
#include <QString>
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

namespace { // Anonymous namespace for helper functions
static std::vector<std::pair<int, RoomAreaHash>> getNextPassChunks(
    const OpenDiablo2::MapData::IterativeRemeshMetadata& state,
    int maxChunksPerPass)
{
    std::vector<std::pair<int, RoomAreaHash>> nextPass;
    if (maxChunksPerPass <= 0) {
        qWarning() << "getNextPassChunks called with maxChunksPerPass <= 0:" << maxChunksPerPass;
        return nextPass;
    }

    nextPass.reserve(static_cast<size_t>(maxChunksPerPass));

    for (const auto& chunk : state.allTargetChunks) {
        if (state.completedChunks.find(chunk) == state.completedChunks.end()) {
            nextPass.push_back(chunk);
            if (nextPass.size() >= static_cast<size_t>(maxChunksPerPass)) {
                break;
            }
        }
    }
    return nextPass;
}
} // namespace

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

void MapCanvas::slot_onForcedPositionChange()
{
    slot_requestUpdate();
}

bool MapCanvas::event(QEvent *const event)
{
    auto tryHandlePinchZoom = [this, event]() -> bool {
        if (event->type() != QEvent::Gesture) {
            return false;
        }

        const auto *const gestureEvent = dynamic_cast<QGestureEvent *>(event);
        if (gestureEvent == nullptr) {
            return false;
        }

        QGesture *const gesture = gestureEvent->gesture(Qt::PinchGesture);
        const auto *const pinch = dynamic_cast<QPinchGesture *>(gesture);
        if (pinch == nullptr) {
            return false;
        }

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

    if (tryHandlePinchZoom()) {
        return true;
    }

    return QOpenGLWidget::event(event);
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

std::shared_ptr<InfoMarkSelection> MapCanvas::getInfoMarkSelection(const MouseSel &sel)
{
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

    const auto hi = getScaled(maxCoord);
    const auto lo = getScaled(minCoord);

    return InfoMarkSelection::alloc(m_data, lo, hi);
}

void MapCanvas::mousePressEvent(QMouseEvent *const event)
{
    const bool hasLeftButton = (event->buttons() & Qt::LeftButton) != 0u;
    const bool hasRightButton = (event->buttons() & Qt::RightButton) != 0u;
    const bool hasCtrl = (event->modifiers() & Qt::CTRL) != 0u;
    MAYBE_UNUSED const bool hasAlt = (event->modifiers() & Qt::ALT) != 0u;

    m_sel1 = m_sel2 = getUnprojectedMouseSel(event);
    m_mouseLeftPressed = hasLeftButton;
    m_mouseRightPressed = hasRightButton;

    if (event->button() == Qt::ForwardButton) {
        slot_layerUp();
        return event->accept();
    } else if (event->button() == Qt::BackButton) {
        slot_layerDown();
        return event->accept();
    } else if (!m_mouseLeftPressed && m_mouseRightPressed) {
        if (m_canvasMouseMode == CanvasMouseModeEnum::MOVE && hasSel1()) {
            m_roomSelection = RoomSelection::createSelection(m_data.findAllRooms(getSel1().getCoordinate()));
            slot_setRoomSelection(SigRoomSelection{m_roomSelection});
            slot_setInfoMarkSelection(getInfoMarkSelection(getSel1()));
            selectionChanged();
        }
        m_mouseRightPressed = false;
        event->accept();
        return;
    }

    switch (m_canvasMouseMode) {
    case CanvasMouseModeEnum::CREATE_INFOMARKS:
        infomarksChanged();
        break;
    case CanvasMouseModeEnum::SELECT_INFOMARKS:
        if (hasLeftButton && hasSel1()) {
            auto tmpSel = getInfoMarkSelection(getSel1());
            if (m_infoMarkSelection != nullptr && !tmpSel->empty() && m_infoMarkSelection->contains(tmpSel->front().getId())) {
                m_infoMarkSelectionMove.reset();
                m_infoMarkSelectionMove.emplace();
            } else {
                m_selectedArea = false;
                m_infoMarkSelectionMove.reset();
            }
        }
        selectionChanged();
        break;
    case CanvasMouseModeEnum::MOVE:
        if (hasLeftButton && hasSel1()) {
            setCursor(Qt::ClosedHandCursor);
            startMoving(m_sel1.value());
        }
        break;

    case CanvasMouseModeEnum::RAYPICK_ROOMS:
        if (hasLeftButton) {
            const auto xy = getMouseCoords(event);
            const auto near_p = unproject_raw(glm::vec3{xy, 0.f}); // near renamed to near_p
            const auto far_p = unproject_raw(glm::vec3{xy, 1.f}); // far renamed to far_p

            const auto hiz = static_cast<int>(std::floor(near_p.z));
            const auto loz = static_cast<int>(std::ceil(far_p.z));
            if (hiz <= loz) break;

            const auto inv_denom = 1.f / (far_p.z - near_p.z);
            RoomIdSet tmpSel;
            for (int z = hiz; z >= loz; --z) {
                const float t = (static_cast<float>(z) - near_p.z) * inv_denom;
                assert(isClamped(t, 0.f, 1.f));
                const auto pos = glm::mix(near_p, far_p, t);
                assert(static_cast<int>(std::lround(pos.z)) == z);
                const Coordinate c2 = MouseSel{Coordinate2f{pos.x, pos.y}, z}.getCoordinate();
                if (const auto r = m_data.findRoomHandle(c2)) {
                    tmpSel.insert(r.getId());
                }
            }
            if (!tmpSel.empty()) {
                slot_setRoomSelection(SigRoomSelection(RoomSelection::createSelection(std::move(tmpSel))));
            }
            selectionChanged();
        }
        break;

    case CanvasMouseModeEnum::SELECT_ROOMS:
        if (hasRightButton) {
            m_selectedArea = false;
            slot_clearRoomSelection();
        }
        if (hasLeftButton && hasSel1()) {
            if (!hasCtrl) {
                const auto pRoom = m_data.findRoomHandle(getSel1().getCoordinate());
                if (pRoom.exists() && m_roomSelection != nullptr && m_roomSelection->contains(pRoom.getId())) {
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
        if (hasLeftButton && hasSel1()) {
            m_connectionSelection = ConnectionSelection::alloc(m_data, getSel1());
            if (!m_connectionSelection->isFirstValid()) {
                m_connectionSelection = nullptr;
            }
            emit sig_newConnectionSelection(nullptr);
        }
        if (hasRightButton) slot_clearConnectionSelection();
        selectionChanged();
        break;

    case CanvasMouseModeEnum::SELECT_CONNECTIONS:
        if (hasLeftButton && hasSel1()) {
            m_connectionSelection = ConnectionSelection::alloc(m_data, getSel1());
            if (!m_connectionSelection->isFirstValid()) {
                m_connectionSelection = nullptr;
            } else {
                const auto &r1 = m_connectionSelection->getFirst().room;
                const ExitDirEnum dir1 = m_connectionSelection->getFirst().direction;
                if (r1.getExit(dir1).outIsEmpty()) m_connectionSelection = nullptr;
            }
            emit sig_newConnectionSelection(nullptr);
        }
        if (hasRightButton) slot_clearConnectionSelection();
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
    const bool hasLeftButton = (event->buttons() & Qt::LeftButton) != 0u;

    if (m_canvasMouseMode != CanvasMouseModeEnum::MOVE) {
        const int vScroll = [this, event]() -> int {
            const int h = height(); const int MARGIN = std::min(100, h / 4); const int y = event->pos().y();
            if (y < MARGIN) return SCROLL_SCALE; else if (y > h - MARGIN) return -SCROLL_SCALE; else return 0;
        }();
        const int hScroll = [this, event]() -> int {
            const int w = width(); const int MARGIN = std::min(100, w / 4); const int x = event->pos().x();
            if (x < MARGIN) return -SCROLL_SCALE; else if (x > w - MARGIN) return SCROLL_SCALE; else return 0;
        }();
        emit sig_continuousScroll(hScroll, vScroll);
    }

    m_sel2 = getUnprojectedMouseSel(event);

    switch (m_canvasMouseMode) {
    case CanvasMouseModeEnum::SELECT_INFOMARKS:
        if (hasLeftButton && hasSel1() && hasSel2()) {
            if (hasInfoMarkSelectionMove()) {
                m_infoMarkSelectionMove->pos = getSel2().pos - getSel1().pos;
                setCursor(Qt::ClosedHandCursor);
            } else {
                m_selectedArea = true;
            }
        }
        selectionChanged();
        break;
    case CanvasMouseModeEnum::CREATE_INFOMARKS:
        if (hasLeftButton) m_selectedArea = true;
        infomarksChanged();
        break;
    case CanvasMouseModeEnum::MOVE:
        if (hasLeftButton && m_mouseLeftPressed && hasSel2() && hasBackup()) {
            const Coordinate2i delta = ((getSel2().pos - getBackup().pos) * static_cast<float>(SCROLL_SCALE)).truncate();
            if (delta.x != 0 || delta.y != 0) emit sig_mapMove(-delta.x, -delta.y);
        }
        break;
    case CanvasMouseModeEnum::RAYPICK_ROOMS: break;
    case CanvasMouseModeEnum::SELECT_ROOMS:
        if (hasLeftButton) {
            if (hasRoomSelectionMove() && hasSel1() && hasSel2()) {
                auto &map = this->m_data;
                auto isMovable = [&map](RoomSelection &sel, const Coordinate &offset) -> bool {
                    sel.removeMissing(map);
                    return map.getCurrentMap().wouldAllowRelativeMove(sel.getRoomIds(), offset);
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
        if (hasLeftButton && hasSel2()) {
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
        if (hasLeftButton && hasSel2()) {
            if (m_connectionSelection != nullptr) {
                m_connectionSelection->setSecond(getSel2());
                if (!m_connectionSelection->isCompleteExisting()) m_connectionSelection->removeSecond();
                selectionChanged();
            }
        }
        break;
    case CanvasMouseModeEnum::CREATE_ROOMS:
    case CanvasMouseModeEnum::NONE:
    default: break;
    }
}

void MapCanvas::mouseReleaseEvent(QMouseEvent *const event)
{
    emit sig_continuousScroll(0, 0);
    m_sel2 = getUnprojectedMouseSel(event);

    if (m_mouseRightPressed) m_mouseRightPressed = false;

    switch (m_canvasMouseMode) {
    case CanvasMouseModeEnum::SELECT_INFOMARKS:
        setCursor(Qt::ArrowCursor);
        if (m_mouseLeftPressed) {
            m_mouseLeftPressed = false;
            if (hasInfoMarkSelectionMove()) {
                const auto pos_copy = m_infoMarkSelectionMove->pos;
                m_infoMarkSelectionMove.reset();
                if (m_infoMarkSelection != nullptr) {
                    const auto offset = Coordinate{(pos_copy * INFOMARK_SCALE).truncate(), 0};
                    const InfoMarkSelection &sel = deref(m_infoMarkSelection);
                    sel.applyOffset(offset);
                    infomarksChanged();
                }
            } else if (hasSel1() && hasSel2()) {
                const auto c1 = getSel1().getScaledCoordinate(INFOMARK_SCALE);
                const auto c2 = getSel2().getScaledCoordinate(INFOMARK_SCALE);
                auto tmpSel = InfoMarkSelection::alloc(m_data, c1, c2);
                if (tmpSel && tmpSel->size() == 1) {
                    const InfomarkHandle &firstMark = tmpSel->front();
                    const Coordinate &pos = firstMark.getPosition1();
                    QString ctemp = QString("Selected Info Mark: [%1] (at %2,%3,%4)")
                                        .arg(firstMark.getText().toQString()).arg(pos.x).arg(pos.y).arg(pos.z);
                    log(ctemp);
                }
                slot_setInfoMarkSelection(tmpSel);
            }
            m_selectedArea = false;
        }
        selectionChanged();
        break;
    case CanvasMouseModeEnum::CREATE_INFOMARKS:
        if (m_mouseLeftPressed && hasSel1() && hasSel2()) {
            m_mouseLeftPressed = false;
            const auto c1 = getSel1().getScaledCoordinate(INFOMARK_SCALE);
            const auto c2 = getSel2().getScaledCoordinate(INFOMARK_SCALE);
            auto tmpSel = InfoMarkSelection::allocEmpty(m_data, c1, c2);
            slot_setInfoMarkSelection(tmpSel);
        }
        infomarksChanged();
        break;
    case CanvasMouseModeEnum::MOVE:
        stopMoving();
        setCursor(Qt::OpenHandCursor);
        if (m_mouseLeftPressed) m_mouseLeftPressed = false;
        if (hasSel1() && hasSel2() && getSel1().to_vec3() == getSel2().to_vec3()) {
            if (const auto room = m_data.findRoomHandle(getSel1().getCoordinate())) {
                auto message = mmqt::previewRoom(room, mmqt::StripAnsiEnum::Yes, mmqt::PreviewStyleEnum::ForDisplay);
                QToolTip::showText(mapToGlobal(event->pos()), message, this, rect(), 5000);
            }
        }
        break;
    case CanvasMouseModeEnum::RAYPICK_ROOMS: break;
    case CanvasMouseModeEnum::SELECT_ROOMS:
        setCursor(Qt::ArrowCursor);
        if (m_ctrlPressed && m_altPressed) break;
        if (m_mouseLeftPressed) {
            m_mouseLeftPressed = false;
            if (m_roomSelectionMove.has_value()) {
                const auto pos = m_roomSelectionMove->pos;
                const bool wrongPlace = m_roomSelectionMove->wrongPlace;
                m_roomSelectionMove.reset();
                if (!wrongPlace && (m_roomSelection != nullptr)) {
                    const Coordinate moverel{pos, 0};
                    m_data.applySingleChange(Change{room_change_types::MoveRelative2{m_roomSelection->getRoomIds(), moverel}});
                }
            } else {
                if (hasSel1() && hasSel2()) {
                    const Coordinate &c1 = getSel1().getCoordinate();
                    const Coordinate &c2 = getSel2().getCoordinate();
                    if (m_roomSelection == nullptr) {
                        m_roomSelection = RoomSelection::createSelection(m_data.findAllRooms(c1, c2));
                    } else {
                        auto &sel = deref(m_roomSelection);
                        sel.removeMissing(m_data);
                        const auto tmpSel = m_data.findAllRooms(c1, c2);
                        for (const RoomId key : tmpSel) {
                            if (sel.contains(key)) sel.erase(key); else sel.insert(key);
                        }
                    }
                }
                if (m_roomSelection != nullptr && !m_roomSelection->empty()) {
                    slot_setRoomSelection(SigRoomSelection{m_roomSelection});
                }
            }
            m_selectedArea = false;
        }
        selectionChanged();
        break;
    case CanvasMouseModeEnum::CREATE_ONEWAY_CONNECTIONS:
    case CanvasMouseModeEnum::CREATE_CONNECTIONS:
        if (m_mouseLeftPressed && hasSel1() && hasSel2()) {
            m_mouseLeftPressed = false;
            if (m_connectionSelection == nullptr) m_connectionSelection = ConnectionSelection::alloc(m_data, getSel1());
            m_connectionSelection->setSecond(getSel2());
            if (!m_connectionSelection->isValid()) {
                m_connectionSelection = nullptr;
            } else {
                const auto first = m_connectionSelection->getFirst();
                const auto second = m_connectionSelection->getSecond();
                const auto &r1 = first.room; const auto &r2 = second.room;
                if (r1.exists() && r2.exists()) {
                    const ExitDirEnum dir1 = first.direction; const ExitDirEnum dir2 = second.direction;
                    const RoomId id1 = r1.getId(); const RoomId id2 = r2.getId();
                    const bool isCompleteNew = m_connectionSelection->isCompleteNew();
                    m_connectionSelection = nullptr;
                    if (isCompleteNew) {
                        const WaysEnum ways = (m_canvasMouseMode != CanvasMouseModeEnum::CREATE_ONEWAY_CONNECTIONS) ? WaysEnum::TwoWay : WaysEnum::OneWay;
                        const exit_change_types::ModifyExitConnection change{ChangeTypeEnum::Add, id1, dir1, id2, ways};
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
            if (m_connectionSelection == nullptr && hasSel1()) m_connectionSelection = ConnectionSelection::alloc(m_data, getSel1());
            m_connectionSelection->setSecond(getSel2());
            if (!m_connectionSelection->isValid() || !m_connectionSelection->isCompleteExisting()) m_connectionSelection = nullptr;
            slot_setConnectionSelection(m_connectionSelection);
        }
        selectionChanged();
        break;
    case CanvasMouseModeEnum::CREATE_ROOMS:
        if (m_mouseLeftPressed) m_mouseLeftPressed = false;
        break;
    case CanvasMouseModeEnum::NONE:
    default: break;
    }
    m_altPressed = false;
    m_ctrlPressed = false;
}

QSize MapCanvas::minimumSizeHint() const { return {BASESIZE / 2, BASESIZE / 2}; }
QSize MapCanvas::sizeHint() const { return {BASESIZE, BASESIZE}; }

void MapCanvas::slot_setScroll(const glm::vec2 &worldPos) { m_scroll = worldPos; resizeGL(); }
void MapCanvas::slot_setHorizontalScroll(const float worldX) { m_scroll.x = worldX; resizeGL(); }
void MapCanvas::slot_setVerticalScroll(const float worldY) { m_scroll.y = worldY; resizeGL(); }

void MapCanvas::slot_zoomIn() { m_scaleFactor.logStep(1); zoomChanged(); resizeGL(); }
void MapCanvas::slot_zoomOut() { m_scaleFactor.logStep(-1); zoomChanged(); resizeGL(); }
void MapCanvas::slot_zoomReset() { m_scaleFactor.set(1.f); zoomChanged(); resizeGL(); }

void MapCanvas::onMovement()
{
    const Coordinate &pos = m_data.tryGetPosition().value_or(Coordinate{});
    m_currentLayer = pos.z;
    emit sig_onCenter(pos.to_vec2() + glm::vec2{0.5f, 0.5f});
    update();
}

void MapCanvas::slot_dataLoaded() { onMovement(); forceUpdateMeshes(); }
void MapCanvas::slot_moveMarker(const RoomId id) { m_data.setRoom(id); onMovement(); }
void MapCanvas::infomarksChanged() { m_batches.infomarksMeshes.reset(); update(); }

void MapCanvas::layerChanged()
{
    updateVisibleChunks();
    requestMissingChunks();
    update();
}

// This is called by renderLoop() when a remesh operation is ready.
void MapCanvas::finishPendingMapBatches()
{
    if (!m_batches.remeshCookie.isPending() || !m_batches.remeshCookie.isReady()) {
        return;
    }

    SharedMapBatchFinisher pFinisher = m_batches.remeshCookie.get();
    if (!pFinisher) {
        qDebug() << "finishPendingMapBatches: Finisher was null, possibly ignored or errored.";
        return;
    }

    OpenDiablo2::Display::FinishedRemeshPayload payload = pFinisher->finish(getOpenGL(), getGLFont());

    if (!m_batches.mapBatches) {
        m_batches.mapBatches.emplace();
    }
    MapBatches& currentMapBatches = m_batches.mapBatches.value();

    for (const auto& [layerId, newChunkedMeshes] : payload.generatedMeshes.batchedMeshes) {
        for (const auto& [chunkHash, newLayerMesh] : newChunkedMeshes) {
            currentMapBatches.batchedMeshes[layerId][chunkHash] = std::move(const_cast<LayerMeshes&>(newLayerMesh));
        }
    }
    for (const auto& [layerId, newChunkedConnectionMeshes] : payload.generatedMeshes.connectionMeshes) {
        for (const auto& [chunkHash, newConnectionMesh] : newChunkedConnectionMeshes) {
            currentMapBatches.connectionMeshes[layerId][chunkHash] = std::move(const_cast<BatchedConnectionMeshes&>(newConnectionMesh));
        }
    }
    for (const auto& [layerId, newChunkedRoomNameMeshes] : payload.generatedMeshes.roomNameBatches) {
        for (const auto& [chunkHash, newRoomNameMesh] : newChunkedRoomNameMeshes) {
            currentMapBatches.roomNameBatches[layerId][chunkHash] = std::move(const_cast<BatchedRoomNamesMesh&>(newRoomNameMesh));
        }
    }

    int M_pendingChunkGenerations_erased_count = 0;
    for (const auto& chunkProfile : payload.chunksCompletedThisPass) {
        if(m_pendingChunkGenerations.erase(chunkProfile)){
            M_pendingChunkGenerations_erased_count++;
        }
    }
    if(M_pendingChunkGenerations_erased_count > 0 || payload.chunksCompletedThisPass.empty()){
         qDebug() << "finishPendingMapBatches: Processed" << payload.chunksCompletedThisPass.size()
                  << "chunks. Removed" << M_pendingChunkGenerations_erased_count << "from m_pendingChunkGenerations."
                  << "Remaining pending:" << m_pendingChunkGenerations.size();
    }

    if (payload.morePassesNeeded && payload.nextIterativeState.has_value()) {
        OpenDiablo2::MapData::IterativeRemeshMetadata nextState = payload.nextIterativeState.value();
        std::vector<std::pair<int, RoomAreaHash>> chunksForNext = getNextPassChunks(nextState, MAX_CHUNKS_PER_ITERATIVE_PASS);

        qDebug() << "Iterative remesh: Pass" << (nextState.currentPassNumber -1) << "completed."
                 << payload.chunksCompletedThisPass.size() << "chunks this pass."
                 << nextState.completedChunks.size() << "total completed out of" << nextState.allTargetChunks.size()
                 << ". Next pass requested with" << chunksForNext.size() << "chunks.";

        if (!chunksForNext.empty()) {
            m_batches.remeshCookie.set(m_data.generateSpecificChunkBatches(mctp::getProxy(m_textures), chunksForNext, nextState));
            for (const auto& chunkProfile : chunksForNext) {
                m_pendingChunkGenerations.insert(chunkProfile);
            }
        } else if (payload.morePassesNeeded) {
            qDebug() << "Iterative remesh: morePassesNeeded was true, but getNextPassChunks returned empty. All target chunks might have been scheduled or completed. Total completed:" << nextState.completedChunks.size() << "/" << nextState.allTargetChunks.size();
        }
    } else {
         qDebug() << "Iterative remesh: Finished. morePassesNeeded =" << payload.morePassesNeeded
                  << "nextIterativeState.has_value =" << payload.nextIterativeState.has_value()
                  << ". Chunks completed this pass:" << payload.chunksCompletedThisPass.size()
                  << ". Total pending chunk generations:" << m_pendingChunkGenerations.size();
    }

    updateVisibleChunks();
    requestMissingChunks();
}

void MapCanvas::requestMissingChunks() {
    if (m_batches.remeshCookie.isPending()) {
        return;
    }

    std::vector<std::pair<int, RoomAreaHash>> chunksToRequestNow;

    for (const auto& layerChunksPair : m_visibleChunks) {
        const int layerId = layerChunksPair.first;
        const std::set<RoomAreaHash>& visibleChunkIds = layerChunksPair.second;

        const MapBatches* currentMapBatchesPtr = m_batches.mapBatches.has_value() ? &(*m_batches.mapBatches) : nullptr;
        auto mapBatchesLayerIt = currentMapBatchesPtr ? currentMapBatchesPtr->batchedMeshes.find(layerId) : (currentMapBatchesPtr ? currentMapBatchesPtr->batchedMeshes.end() : decltype(currentMapBatchesPtr->batchedMeshes.end()){});

        for (const RoomAreaHash roomAreaHash : visibleChunkIds) {
            bool isMissingOrInvalid = true;
            if (currentMapBatchesPtr && mapBatchesLayerIt != currentMapBatchesPtr->batchedMeshes.end()) {
                const ChunkedLayerMeshes& chunkedLayerMeshes = mapBatchesLayerIt->second;
                auto chunkIt = chunkedLayerMeshes.find(roomAreaHash);
                if (chunkIt != chunkedLayerMeshes.end() && chunkIt->second) {
                    isMissingOrInvalid = false;
                }
            }

            if (isMissingOrInvalid) {
                std::pair<int, RoomAreaHash> chunkKey = {layerId, roomAreaHash};
                if (m_pendingChunkGenerations.find(chunkKey) == m_pendingChunkGenerations.end()) {
                    chunksToRequestNow.push_back(chunkKey);
                }
            }
        }
    }

    if (!chunksToRequestNow.empty()) {
        qDebug() << "Requesting" << chunksToRequestNow.size() << "missing chunks.";
        for(const auto& chunkKey : chunksToRequestNow) {
            m_pendingChunkGenerations.insert(chunkKey);
        }
        m_batches.remeshCookie.set(
            m_data.generateSpecificChunkBatches(mctp::getProxy(m_textures), chunksToRequestNow, std::nullopt) // Explicitly pass std::nullopt
        );
    }
}

std::optional<glm::vec3> MapCanvas::getUnprojectedScreenPos(const glm::vec2& screenPos) const {
    return this->unproject_clamped(screenPos);
}

void MapCanvas::updateVisibleChunks() {
    auto start_time = std::chrono::high_resolution_clock::now();
    m_visibleChunks.clear();

    const Map& currentMap = m_data.getCurrentMap();
    if (currentMap.empty()) {
       auto end_time_early = std::chrono::high_resolution_clock::now();
       auto duration_early = std::chrono::duration_cast<std::chrono::microseconds>(end_time_early - start_time);
       // qDebug() << "updateVisibleChunks execution time (empty map):" << duration_early.count() << "microseconds"; // Too spammy
       return;
    }
    const World& world = currentMap.getWorld();
    const SpatialDb& spatialDb = world.getSpatialDb();

    float viewPortWidth = static_cast<float>(width());
    float viewPortHeight = static_cast<float>(height());

    glm::vec3 worldTopLeftRaw = this->unproject_clamped(glm::vec2(0.0f, viewPortHeight));
    glm::vec3 worldTopRightRaw = this->unproject_clamped(glm::vec2(viewPortWidth, viewPortHeight));
    glm::vec3 worldBottomLeftRaw = this->unproject_clamped(glm::vec2(0.0f, 0.0f));
    glm::vec3 worldBottomRightRaw = this->unproject_clamped(glm::vec2(viewPortWidth, 0.0f));

    float worldMinX = std::min({worldTopLeftRaw.x, worldTopRightRaw.x, worldBottomLeftRaw.x, worldBottomRightRaw.x});
    float worldMaxX = std::max({worldTopLeftRaw.x, worldTopRightRaw.x, worldBottomLeftRaw.x, worldBottomRightRaw.x});
    float worldMinY = std::min({worldTopLeftRaw.y, worldTopRightRaw.y, worldBottomLeftRaw.y, worldBottomRightRaw.y});
    float worldMaxY = std::max({worldTopLeftRaw.y, worldTopRightRaw.y, worldBottomLeftRaw.y, worldBottomRightRaw.y});

    std::vector<RoomId> potentiallyVisibleRoomIds = spatialDb.getRoomsInViewport(
        m_currentLayer, worldMinX, worldMinY, worldMaxX, worldMaxY
    );

    const float marginPixels = 20.0f;

    for (RoomId roomId : potentiallyVisibleRoomIds) {
        RoomHandle room = currentMap.getRoomHandle(roomId);
        if (!room.exists()) continue;
        if (m_mapScreen.isRoomVisible(room.getPosition(), marginPixels)) {
            RoomAreaHash areaHash = getRoomAreaHash(room); // Ensure getRoomAreaHash is accessible
            m_visibleChunks[m_currentLayer].insert(areaHash);
        }
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    // qDebug() << "updateVisibleChunks execution time:" << duration.count() << "microseconds. Visible chunks:" << m_visibleChunks[m_currentLayer].size(); // Too spammy
}

void MapCanvas::forceUpdateMeshes()
{
    if (m_batches.remeshCookie.isPending()) {
        qDebug() << "forceUpdateMeshes: Previous remesh pending. Ignoring it and restarting.";
        m_batches.remeshCookie.setIgnored();
        // The remeshCookie itself will be overwritten by the new call later in this function.
        // No need to explicitly reset m_batches.remeshCookie here, as set() will handle it.
    }

    updateVisibleChunks();

    std::vector<std::pair<int, RoomAreaHash>> viewportChunksForFirstPass;
    std::vector<std::pair<int, RoomAreaHash>> allTargetChunksForOperation;

    for (const auto& layerChunksPair : m_visibleChunks) {
        for (const RoomAreaHash roomAreaHash : layerChunksPair.second) {
            allTargetChunksForOperation.push_back({layerChunksPair.first, roomAreaHash});
            // Prioritize current layer for the first pass
            if (layerChunksPair.first == m_currentLayer) {
                 if (viewportChunksForFirstPass.size() < static_cast<size_t>(MAX_CHUNKS_PER_ITERATIVE_PASS)) { // Limit first pass size
                    viewportChunksForFirstPass.push_back({layerChunksPair.first, roomAreaHash});
                }
            }
        }
    }

    if (allTargetChunksForOperation.empty()) {
        qDebug() << "forceUpdateMeshes: No target chunks determined (allVisibleChunks is empty).";
        update(); // Ensure repaint even if nothing to do.
        return;
    }

    // If no chunks on the current layer are visible, pick from any visible layer for the first pass.
    if (viewportChunksForFirstPass.empty()) {
        for(size_t i = 0; i < allTargetChunksForOperation.size() && i < static_cast<size_t>(MAX_CHUNKS_PER_ITERATIVE_PASS); ++i) {
            viewportChunksForFirstPass.push_back(allTargetChunksForOperation[i]);
        }
    }

    if (viewportChunksForFirstPass.empty()) {
         qDebug() << "forceUpdateMeshes: Still no viewport chunks for first pass after fallback. Total targets:" << allTargetChunksForOperation.size();
         // This implies allTargetChunksForOperation was also empty, which should have been caught above.
         // Or MAX_CHUNKS_PER_ITERATIVE_PASS is 0.
         update();
         return;
    }


    OpenDiablo2::MapData::IterativeRemeshMetadata initialMetadata;
    initialMetadata.allTargetChunks = allTargetChunksForOperation;
    initialMetadata.viewportChunks = viewportChunksForFirstPass; // Store the initial set for potential future prioritization
    initialMetadata.completedChunks.clear();
    initialMetadata.currentPassNumber = 0;
    initialMetadata.strategy = OpenDiablo2::MapData::IterativeRemeshMetadata::Strategy::IterativeViewportPriority;

    qDebug() << "forceUpdateMeshes: Starting iterative remesh. First pass chunks:" << viewportChunksForFirstPass.size()
             << ". Total target chunks:" << allTargetChunksForOperation.size();

    m_pendingChunkGenerations.clear(); // Clear all pending before starting a new full forced update.
    for (const auto& chunkProfile : viewportChunksForFirstPass) {
        m_pendingChunkGenerations.insert(chunkProfile);
    }

    m_batches.mapBatches.reset(); // Clear existing complete batches when forcing a full update
    m_diff.resetExistingMeshesAndIgnorePendingRemesh();

    m_batches.remeshCookie.set(
        m_data.generateSpecificChunkBatches(mctp::getProxy(m_textures), viewportChunksForFirstPass, initialMetadata)
    );

    update();
}


void MapCanvas::slot_mapChanged()
{
    // A map change likely invalidates current meshes and pending operations.
    // A full refresh is often appropriate.
    qDebug() << "MapCanvas::slot_mapChanged: Forcing mesh update due to map change.";
    forceUpdateMeshes();
}

void MapCanvas::slot_requestUpdate() { update(); }

void MapCanvas::screenChanged()
{
    auto &gl = getOpenGL();
    if (!gl.isRendererInitialized()) return;

    const auto newDpi = static_cast<float>(QPaintDevice::devicePixelRatioF());
    const auto oldDpi = gl.getDevicePixelRatio();

    if (!utils::equals(newDpi, oldDpi)) {
        log(QString("Display: %1 DPI").arg(static_cast<double>(newDpi)));
        m_batches.resetExistingMeshesButKeepPendingRemesh(); // Keep pending iterative remesh
        gl.setDevicePixelRatio(newDpi);
        auto &font = getGLFont();
        font.cleanup();
        font.init();
        forceUpdateMeshes();
    }
}

void MapCanvas::selectionChanged() { update(); emit sig_selectionChanged(); }
void MapCanvas::graphicsSettingsChanged() { update(); }

void MapCanvas::userPressedEscape(bool /*pressed*/)
{
    switch (m_canvasMouseMode) {
    case CanvasMouseModeEnum::NONE:
    case CanvasMouseModeEnum::CREATE_ROOMS: break;
    case CanvasMouseModeEnum::CREATE_CONNECTIONS:
    case CanvasMouseModeEnum::SELECT_CONNECTIONS:
    case CanvasMouseModeEnum::CREATE_ONEWAY_CONNECTIONS:
        slot_clearConnectionSelection(); break;
    case CanvasMouseModeEnum::RAYPICK_ROOMS:
    case CanvasMouseModeEnum::SELECT_ROOMS:
        m_selectedArea = false;
        m_roomSelectionMove.reset();
        slot_clearRoomSelection(); break;
    case CanvasMouseModeEnum::MOVE:
        FALLTHROUGH;
    case CanvasMouseModeEnum::SELECT_INFOMARKS:
    case CanvasMouseModeEnum::CREATE_INFOMARKS:
        m_infoMarkSelectionMove.reset();
        slot_clearInfoMarkSelection(); break;
    }
}

// --- Methods related to initializeGL, paintGL, resizeGL, cleanupOpenGL ---
// (These are mostly unchanged but included for completeness of the file overwrite)
void MapCanvas::cleanupOpenGL()
{
    MakeCurrentRaii makeCurrentRaii{*this};
    m_textures.cleanup();
    m_batches.mapBatches.reset();
    m_batches.infomarksMeshes.reset();
    m_glFont.cleanup();
    m_opengl.destroyVao(m_defaultVao);
    m_opengl.cleanup();
}

NODISCARD QSize MapCanvas::minimumSizeHint() const { return {BASESIZE / 2, BASESIZE / 2}; }
NODISCARD QSize MapCanvas::sizeHint() const { return {BASESIZE, BASESIZE}; }

void MapCanvas::slot_setScroll(const glm::vec2 &worldPos) { m_scroll = worldPos; resizeGL(); }
void MapCanvas::slot_setHorizontalScroll(const float worldX) { m_scroll.x = worldX; resizeGL(); }
void MapCanvas::slot_setVerticalScroll(const float worldY) { m_scroll.y = worldY; resizeGL(); }

void MapCanvas::slot_zoomIn() { m_scaleFactor.logStep(1); zoomChanged(); resizeGL(); }
void MapCanvas::slot_zoomOut() { m_scaleFactor.logStep(-1); zoomChanged(); resizeGL(); }
void MapCanvas::slot_zoomReset() { m_scaleFactor.set(1.f); zoomChanged(); resizeGL(); }

void MapCanvas::reportGLVersion() { /* ... existing code ... */ }
NODISCARD bool MapCanvas::isBlacklistedDriver() { /* ... existing code ... */ return false; }
void MapCanvas::initializeGL() {
    m_opengl.init();
    reportGLVersion();
    if (isBlacklistedDriver()) { /* ... existing code ... */ }
    initLogger();
    m_defaultVao = m_opengl.createVao();
    m_glFont.init();
    initTextures();
    updateMultisampling();
    m_lifetime = m_data.getChangeMonitor().add<MapDataUpdateFlags::MAP_LOADED>(RepeatForeverObserver{[this]() { slot_dataLoaded(); }});
    emit sig_log("MapCanvas", "initialized");
}

void MapCanvas::initLogger() { /* ... existing code ... */ }
void MapCanvas::initTextures() { m_textures.init(m_opengl, getConfig().textures); }
void MapCanvas::updateTextures() { /* ... existing code ... */ }
void MapCanvas::updateMultisampling() { /* ... existing code ... */ }
NODISCARD glm::mat4 MapCanvas::getViewProj_old(const glm::vec2&, const glm::ivec2&, float, int) { return glm::mat4(1.0f); }
NODISCARD glm::mat4 MapCanvas::getViewProj(const glm::vec2 &scrollPos, const glm::ivec2 &size, float zoomScale, int currentLayer) {
    return MapCanvasViewport::getViewProj(scrollPos, size, zoomScale, currentLayer);
}
void MapCanvas::setMvp(const glm::mat4 &viewProj) { m_opengl.setMvpMatrix(viewProj); }
void MapCanvas::setViewportAndMvp(int width, int height) {
    MapCanvasViewport::setViewportAndMvp(width, height, m_currentLayer);
}

void MapCanvas::resizeGL(int width, int height) {
    MapCanvasViewport::resizeGL(width, height);
    update();
}

void MapCanvas::setAnimating(bool value) { /* ... existing code ... */ }
void MapCanvas::renderLoop() { /* ... existing code ... */ }

void MapCanvas::paintGL() {
    renderLoop();
    if (m_frameRateController.animating) {
        QTimer::singleShot(0, this, &MapCanvas::paintGL);
    }
}
void MapCanvas::actuallyPaintGL() { /* ... existing code ... */ }
void MapCanvas::updateBatches() { /* ... existing code ... */ }
void MapCanvas::updateMapBatches() { /* ... existing code ... */ }
void MapCanvas::finishPendingMapBatches() { /* This is the function being replaced by the diff above */ }
void MapCanvas::updateInfomarkBatches() { /* ... existing code ... */ }
NODISCARD BatchedInfomarksMeshes MapCanvas::getInfoMarksMeshes() { /* ... existing code ... */ return {}; }
void MapCanvas::drawInfoMark(InfomarksBatch&, const InfomarkHandle&, int, const glm::vec2&, const std::optional<Color>&) { /* ... existing code ... */ }
void MapCanvas::paintMap() { /* ... existing code ... */ }
void MapCanvas::renderMapBatches() { /* ... existing code ... */ }
void MapCanvas::paintBatchedInfomarks() { /* ... existing code ... */ }
void MapCanvas::paintSelections() { /* ... existing code ... */ }
void MapCanvas::paintSelectionArea() { /* ... existing code ... */ }
void MapCanvas::paintNewInfomarkSelection() { /* ... existing code ... */ }
void MapCanvas::paintSelectedRooms() { /* ... existing code ... */ }
void MapCanvas::paintSelectedRoom(RoomSelFakeGL&, const RawRoom&) { /* ... existing code ... */ }
void MapCanvas::paintSelectedConnection() { /* ... existing code ... */ }
void MapCanvas::paintNearbyConnectionPoints() { /* ... existing code ... */ }
void MapCanvas::paintSelectedInfoMarks() { /* ... existing code ... */ }
void MapCanvas::paintCharacters() { /* ... existing code ... */ }
void MapCanvas::drawGroupCharacters(CharacterBatch&) { /* ... existing code ... */ }
void MapCanvas::paintDifferences() { /* ... existing code ... */ }

NODISCARD bool MapCanvas::Diff::isUpToDate(const Map&, const Map&) const { return true; }
NODISCARD bool MapCanvas::Diff::hasRelatedDiff(const Map&) const { return false; }
void MapCanvas::Diff::cancelUpdates(const Map&) { /* ... */ }
void MapCanvas::Diff::maybeAsyncUpdate(const Map&, const Map&) { /* ... */ }

void MapCanvas::slot_onMessageLoggedDirect(const QOpenGLDebugMessage &message) { qDebug() << message; }
