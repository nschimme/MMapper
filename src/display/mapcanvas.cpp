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
#include "InfoMarkSelection.h"
#include "MapCanvasData.h"
#include "MapCanvasRoomDrawer.h" // For Batches
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

    // Provide MapData with a pointer to MapCanvas's batches
    m_data.setMapCanvasBatches(m_batches.mapBatches ? &(*m_batches.mapBatches) : nullptr);
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
            if (event->angleDelta().y() > 100) slot_layerDown();
            else if (event->angleDelta().y() < -100) slot_layerUp();
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

bool MapCanvas::event(QEvent *const event)
{
    auto tryHandlePinchZoom = [this, event]() -> bool {
        if (event->type() != QEvent::Gesture) return false;
        const auto *const gestureEvent = dynamic_cast<QGestureEvent *>(event);
        if (!gestureEvent) return false;
        QGesture *const gesture = gestureEvent->gesture(Qt::PinchGesture);
        const auto *const pinch = dynamic_cast<QPinchGesture *>(gesture);
        if (!pinch) return false;
        const QPinchGesture::ChangeFlags changeFlags = pinch->changeFlags();
        if (changeFlags & QPinchGesture::ScaleFactorChanged) {
            m_scaleFactor.setPinch(static_cast<float>(pinch->totalScaleFactor()));
        }
        if (pinch->state() == Qt::GestureFinished) {
            m_scaleFactor.endPinch();
            zoomChanged();
        }
        resizeGL();
        return true;
    };
    if (tryHandlePinchZoom()) return true;
    return QOpenGLWidget::event(event);
}

void MapCanvas::slot_createRoom()
{
    if (!hasSel1()) return;
    const Coordinate c = getSel1().getCoordinate();
    if (m_data.findRoomHandle(c)) return;
    m_data.createEmptyRoom(Coordinate{c.x, c.y, m_currentLayer});
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
        return InfoMarkSelection::alloc(m_data, pos + Coordinate{-INFOMARK_CLICK_RADIUS, -INFOMARK_CLICK_RADIUS, 0}, pos + Coordinate{+INFOMARK_CLICK_RADIUS, +INFOMARK_CLICK_RADIUS, 0});
    }
    const glm::vec2 clickPoint = glm::vec2{optClickPoint.value()};
    glm::vec3 maxCoord = center;
    glm::vec3 minCoord = center;
    const auto probe = [&](const glm::vec2 &offset){ const auto coord = unproject_clamped(clickPoint + offset); maxCoord = glm::max(maxCoord, coord); minCoord = glm::min(minCoord, coord); };
    for (int dy = -1; dy <= 1; ++dy) for (int dx = -1; dx <= 1; ++dx) probe(glm::vec2{static_cast<float>(dx) * CLICK_RADIUS, static_cast<float>(dy) * CLICK_RADIUS});
    const auto getScaled = [this](const glm::vec3 &c){ const auto pos = glm::ivec3(glm::vec2(c) * static_cast<float>(INFOMARK_SCALE), m_currentLayer); return Coordinate{pos.x, pos.y, pos.z}; };
    return InfoMarkSelection::alloc(m_data, getScaled(minCoord), getScaled(maxCoord));
}

void MapCanvas::mousePressEvent(QMouseEvent *const event) { /* ... content of the first (correct) mousePressEvent ... */
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
    case CanvasMouseModeEnum::CREATE_INFOMARKS: infomarksChanged(); break;
    case CanvasMouseModeEnum::SELECT_INFOMARKS:
        if (hasLeftButton && hasSel1()) {
            auto tmpSel = getInfoMarkSelection(getSel1());
            if (m_infoMarkSelection && !tmpSel->empty() && m_infoMarkSelection->contains(tmpSel->front().getId())) {
                m_infoMarkSelectionMove.emplace();
            } else {
                m_selectedArea = false; m_infoMarkSelectionMove.reset();
            }
        }
        selectionChanged(); break;
    case CanvasMouseModeEnum::MOVE: if (hasLeftButton && hasSel1()) { setCursor(Qt::ClosedHandCursor); startMoving(m_sel1.value()); } break;
    case CanvasMouseModeEnum::RAYPICK_ROOMS:
        if (hasLeftButton) {
            const auto xy = getMouseCoords(event); const auto near_p = unproject_raw(glm::vec3{xy,0.f}); const auto far_p = unproject_raw(glm::vec3{xy,1.f});
            const auto hiz = static_cast<int>(std::floor(near_p.z)); const auto loz = static_cast<int>(std::ceil(far_p.z));
            if (hiz <= loz) break;
            const auto inv_denom = 1.f / (far_p.z - near_p.z);
            RoomIdSet tmpSel;
            for (int z = hiz; z >= loz; --z) {
                const float t = (static_cast<float>(z)-near_p.z)*inv_denom; assert(isClamped(t,0.f,1.f)); const auto pos_mix = glm::mix(near_p,far_p,t);
                assert(static_cast<int>(std::lround(pos_mix.z))==z);
                if(const auto r = m_data.findRoomHandle(MouseSel{Coordinate2f{pos_mix.x,pos_mix.y},z}.getCoordinate())) tmpSel.insert(r.getId());
            }
            if(!tmpSel.empty()) slot_setRoomSelection(SigRoomSelection(RoomSelection::createSelection(std::move(tmpSel))));
            selectionChanged();
        } break;
    case CanvasMouseModeEnum::SELECT_ROOMS:
        if (hasRightButton) { m_selectedArea = false; slot_clearRoomSelection(); }
        if (hasLeftButton && hasSel1()) {
            if (!hasCtrl) {
                const auto pRoom = m_data.findRoomHandle(getSel1().getCoordinate());
                if (pRoom.exists() && m_roomSelection && m_roomSelection->contains(pRoom.getId())) m_roomSelectionMove.emplace(RoomSelMove{});
                else { m_roomSelectionMove.reset(); m_selectedArea = false; slot_clearRoomSelection(); }
            } else m_ctrlPressed = true;
        }
        selectionChanged(); break;
    case CanvasMouseModeEnum::CREATE_ONEWAY_CONNECTIONS: case CanvasMouseModeEnum::CREATE_CONNECTIONS:
        if (hasLeftButton && hasSel1()) { m_connectionSelection = ConnectionSelection::alloc(m_data, getSel1()); if(!m_connectionSelection->isFirstValid()) m_connectionSelection=nullptr; emit sig_newConnectionSelection(nullptr); }
        if (hasRightButton) slot_clearConnectionSelection();
        selectionChanged(); break;
    case CanvasMouseModeEnum::SELECT_CONNECTIONS:
        if (hasLeftButton && hasSel1()) {
            m_connectionSelection = ConnectionSelection::alloc(m_data, getSel1());
            if (!m_connectionSelection->isFirstValid() || m_connectionSelection->getFirst().room.getExit(m_connectionSelection->getFirst().direction).outIsEmpty()) m_connectionSelection = nullptr;
            emit sig_newConnectionSelection(nullptr);
        }
        if (hasRightButton) slot_clearConnectionSelection();
        selectionChanged(); break;
    case CanvasMouseModeEnum::CREATE_ROOMS: slot_createRoom(); break;
    case CanvasMouseModeEnum::NONE: default: break;
    }
}
void MapCanvas::mouseMoveEvent(QMouseEvent *const event) { /* ... content of the first (correct) mouseMoveEvent ... */
    const bool hasLeftButton = (event->buttons() & Qt::LeftButton) != 0u;
    if(m_canvasMouseMode != CanvasMouseModeEnum::MOVE){
        const int vS = [this,event](){int h=height(),M=std::min(100,h/4),y=event->pos().y(); if(y<M)return SCROLL_SCALE; else if(y>h-M)return -SCROLL_SCALE; else return 0;}();
        const int hS = [this,event](){int w=width(),M=std::min(100,w/4),x=event->pos().x(); if(x<M)return -SCROLL_SCALE; else if(x>w-M)return SCROLL_SCALE; else return 0;}();
        emit sig_continuousScroll(hS,vS);
    }
    m_sel2=getUnprojectedMouseSel(event);
    switch(m_canvasMouseMode){
    case CanvasMouseModeEnum::SELECT_INFOMARKS: if(hasLeftButton&&hasSel1()&&hasSel2()){if(hasInfoMarkSelectionMove()){m_infoMarkSelectionMove->pos=getSel2().pos-getSel1().pos;setCursor(Qt::ClosedHandCursor);}else m_selectedArea=true;} selectionChanged(); break;
    case CanvasMouseModeEnum::CREATE_INFOMARKS: if(hasLeftButton)m_selectedArea=true; infomarksChanged(); break;
    case CanvasMouseModeEnum::MOVE: if(hasLeftButton&&m_mouseLeftPressed&&hasSel2()&&hasBackup()){const Coordinate2i d=((getSel2().pos-getBackup().pos)*static_cast<float>(SCROLL_SCALE)).truncate(); if(d.x!=0||d.y!=0)emit sig_mapMove(-d.x,-d.y);} break;
    case CanvasMouseModeEnum::RAYPICK_ROOMS: break;
    case CanvasMouseModeEnum::SELECT_ROOMS: if(hasLeftButton){if(hasRoomSelectionMove()&&hasSel1()&&hasSel2()){auto&m=this->m_data; auto iM=[&m](RoomSelection&s,const Coordinate&o){s.removeMissing(m);return m.getCurrentMap().wouldAllowRelativeMove(s.getRoomIds(),o);}; const auto df=getSel2().pos.truncate()-getSel1().pos.truncate(); const auto wP=!iM(deref(m_roomSelection),Coordinate{df,0}); m_roomSelectionMove->pos=df;m_roomSelectionMove->wrongPlace=wP;setCursor(wP?Qt::ForbiddenCursor:Qt::ClosedHandCursor);}else m_selectedArea=true;} selectionChanged(); break;
    case CanvasMouseModeEnum::CREATE_ONEWAY_CONNECTIONS: case CanvasMouseModeEnum::CREATE_CONNECTIONS: if(hasLeftButton&&hasSel2()){if(m_connectionSelection){m_connectionSelection->setSecond(getSel2());if(m_connectionSelection->isValid()&&!m_connectionSelection->isCompleteNew())m_connectionSelection->removeSecond();selectionChanged();}}else slot_requestUpdate(); break;
    case CanvasMouseModeEnum::SELECT_CONNECTIONS: if(hasLeftButton&&hasSel2()){if(m_connectionSelection){m_connectionSelection->setSecond(getSel2());if(!m_connectionSelection->isCompleteExisting())m_connectionSelection->removeSecond();selectionChanged();}} break;
    case CanvasMouseModeEnum::CREATE_ROOMS: case CanvasMouseModeEnum::NONE: default: break;
    }
}
void MapCanvas::mouseReleaseEvent(QMouseEvent *const event) { /* ... content of the first (correct) mouseReleaseEvent ... */
    emit sig_continuousScroll(0,0); m_sel2=getUnprojectedMouseSel(event); if(m_mouseRightPressed)m_mouseRightPressed=false;
    switch(m_canvasMouseMode){
    case CanvasMouseModeEnum::SELECT_INFOMARKS: setCursor(Qt::ArrowCursor); if(m_mouseLeftPressed){m_mouseLeftPressed=false; if(hasInfoMarkSelectionMove()){const auto p=m_infoMarkSelectionMove->pos;m_infoMarkSelectionMove.reset(); if(m_infoMarkSelection){const auto o=Coordinate{(p*INFOMARK_SCALE).truncate(),0};const InfoMarkSelection&s=deref(m_infoMarkSelection);s.applyOffset(o);infomarksChanged();}}else if(hasSel1()&&hasSel2()){const auto c1=getSel1().getScaledCoordinate(INFOMARK_SCALE);const auto c2=getSel2().getScaledCoordinate(INFOMARK_SCALE); auto tS=InfoMarkSelection::alloc(m_data,c1,c2); if(tS&&tS->size()==1){const auto&fM=tS->front();const auto&p=fM.getPosition1();log(QString("Selected Info Mark: [%1] (at %2,%3,%4)").arg(fM.getText().toQString()).arg(p.x).arg(p.y).arg(p.z));} slot_setInfoMarkSelection(tS);} m_selectedArea=false;} selectionChanged(); break;
    case CanvasMouseModeEnum::CREATE_INFOMARKS: if(m_mouseLeftPressed&&hasSel1()&&hasSel2()){m_mouseLeftPressed=false;const auto c1=getSel1().getScaledCoordinate(INFOMARK_SCALE);const auto c2=getSel2().getScaledCoordinate(INFOMARK_SCALE);slot_setInfoMarkSelection(InfoMarkSelection::allocEmpty(m_data,c1,c2));} infomarksChanged(); break;
    case CanvasMouseModeEnum::MOVE: stopMoving();setCursor(Qt::OpenHandCursor);if(m_mouseLeftPressed)m_mouseLeftPressed=false; if(hasSel1()&&hasSel2()&&getSel1().to_vec3()==getSel2().to_vec3()){if(const auto r=m_data.findRoomHandle(getSel1().getCoordinate())){auto msg=mmqt::previewRoom(r,mmqt::StripAnsiEnum::Yes,mmqt::PreviewStyleEnum::ForDisplay);QToolTip::showText(mapToGlobal(event->pos()),msg,this,rect(),5000);}} break;
    case CanvasMouseModeEnum::RAYPICK_ROOMS: break;
    case CanvasMouseModeEnum::SELECT_ROOMS: setCursor(Qt::ArrowCursor); if(m_ctrlPressed&&m_altPressed)break; if(m_mouseLeftPressed){m_mouseLeftPressed=false;if(m_roomSelectionMove.has_value()){const auto p=m_roomSelectionMove->pos;const bool wP=m_roomSelectionMove->wrongPlace;m_roomSelectionMove.reset();if(!wP&&(m_roomSelection))m_data.applySingleChange(Change{room_change_types::MoveRelative2{m_roomSelection->getRoomIds(),Coordinate{p,0}}});}else{if(hasSel1()&&hasSel2()){const auto&c1=getSel1().getCoordinate();const auto&c2=getSel2().getCoordinate();if(m_roomSelection==nullptr)m_roomSelection=RoomSelection::createSelection(m_data.findAllRooms(c1,c2));else{auto&s=deref(m_roomSelection);s.removeMissing(m_data);const auto tS=m_data.findAllRooms(c1,c2);for(const RoomId k:tS){if(s.contains(k))s.erase(k);else s.insert(k);}}}}if(m_roomSelection&&!m_roomSelection->empty())slot_setRoomSelection(SigRoomSelection{m_roomSelection});}m_selectedArea=false;}selectionChanged();break;
    case CanvasMouseModeEnum::CREATE_ONEWAY_CONNECTIONS: case CanvasMouseModeEnum::CREATE_CONNECTIONS: if(m_mouseLeftPressed&&hasSel1()&&hasSel2()){m_mouseLeftPressed=false;if(m_connectionSelection==nullptr)m_connectionSelection=ConnectionSelection::alloc(m_data,getSel1());m_connectionSelection->setSecond(getSel2());if(!m_connectionSelection->isValid())m_connectionSelection=nullptr;else{const auto f=m_connectionSelection->getFirst();const auto s=m_connectionSelection->getSecond();const auto&r1=f.room;const auto&r2=s.room;if(r1.exists()&&r2.exists()){const auto d1=f.direction;const auto d2=s.direction;const RoomId id1=r1.getId();const RoomId id2=r2.getId();const bool iCN=m_connectionSelection->isCompleteNew();m_connectionSelection=nullptr;if(iCN){const WaysEnum w=(m_canvasMouseMode!=CanvasMouseModeEnum::CREATE_ONEWAY_CONNECTIONS)?WaysEnum::TwoWay:WaysEnum::OneWay;m_data.applySingleChange(Change{exit_change_types::ModifyExitConnection{ChangeTypeEnum::Add,id1,d1,id2,w}});}m_connectionSelection=ConnectionSelection::alloc(m_data);m_connectionSelection->setFirst(id1,d1);m_connectionSelection->setSecond(id2,d2);slot_mapChanged();}}}slot_setConnectionSelection(m_connectionSelection);}selectionChanged();break;
    case CanvasMouseModeEnum::SELECT_CONNECTIONS: if(m_mouseLeftPressed&&(m_connectionSelection||hasSel1())&&hasSel2()){m_mouseLeftPressed=false;if(m_connectionSelection==nullptr&&hasSel1())m_connectionSelection=ConnectionSelection::alloc(m_data,getSel1());m_connectionSelection->setSecond(getSel2());if(!m_connectionSelection->isValid()||!m_connectionSelection->isCompleteExisting())m_connectionSelection=nullptr;slot_setConnectionSelection(m_connectionSelection);}selectionChanged();break;
    case CanvasMouseModeEnum::CREATE_ROOMS: if(m_mouseLeftPressed)m_mouseLeftPressed=false; break;
    case CanvasMouseModeEnum::NONE: default: break;
    }
    m_altPressed=false;m_ctrlPressed=false;
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
void MapCanvas::layerChanged() { updateVisibleRoomAreas(); requestMissingRoomAreas(); update(); }

void MapCanvas::requestMissingRoomAreas() {
    if (m_batches.remeshCookie.isPending()) return;
    std::vector<RoomArea> roomAreasToRequestNow;
    for (const RoomArea& roomArea : m_visibleRoomAreas) {
        bool isLoaded = m_data.isRoomAreaLoaded(roomArea);
        if (!isLoaded) {
            if (m_pendingRoomAreaGenerations.find(roomArea) == m_pendingRoomAreaGenerations.end()) {
                roomAreasToRequestNow.push_back(roomArea);
            }
        }
    }
    if (!roomAreasToRequestNow.empty()) {
        for(const RoomArea& roomArea : roomAreasToRequestNow) m_pendingRoomAreaGenerations.insert(roomArea);
        m_batches.remeshCookie.set(m_data.generateRoomAreaBatches(mctp::getProxy(m_textures), roomAreasToRequestNow));
        m_cookieAssociatedAreas = roomAreasToRequestNow;
    }
}

// This is the first, correct definition of getUnprojectedScreenPos
std::optional<glm::vec3> MapCanvas::getUnprojectedScreenPos(const glm::vec2& screenPos) const {
    return this->unproject_clamped(screenPos);
}

// This is the first, correct definition of updateVisibleRoomAreas
void MapCanvas::updateVisibleRoomAreas() {
    m_visibleRoomAreas.clear();
    const auto tl_opt = getUnprojectedScreenPos(glm::vec2(0.0f, 0.0f));
    const auto br_opt = getUnprojectedScreenPos(glm::vec2(static_cast<float>(width()), static_cast<float>(height())));
    if (!tl_opt || !br_opt) return;
    const glm::vec2 worldTopLeft(std::floor(tl_opt->x), std::floor(tl_opt->y));
    const glm::vec2 worldBottomRight(std::floor(br_opt->x), std::floor(br_opt->y));
    const float minX = worldTopLeft.x;
    const float maxX = worldBottomRight.x;
    const float minY = worldTopLeft.y;
    const float maxY = worldBottomRight.y;
    const int minZ = m_currentLayer - getConfig().canvas.mapRadius[2];
    const int maxZ = m_currentLayer + getConfig().canvas.mapRadius[2];

    for (int z = minZ; z <= maxZ; ++z) {
        Coordinate areaMinCoord(static_cast<int>(std::min(minX, maxX)), static_cast<int>(std::min(minY, maxY)), z);
        Coordinate areaMaxCoord(static_cast<int>(std::max(minX, maxX)), static_cast<int>(std::max(minY, maxY)), z);
        // Corrected call:
        RoomIdSet roomIdsInRect = m_data.getRoomIdsInBoundingBox(Bounds{areaMinCoord, areaMaxCoord}, z);
        for (RoomId roomId : roomIdsInRect) {
            if (auto roomHandle = m_data.findRoomHandle(roomId)) {
                m_visibleRoomAreas.insert(roomHandle.getArea());
            }
        }
    }
    // Erroneous block starting with "bool isMissingOrInvalid" is now removed from here.
}

// This is the first, correct definition of forceUpdateMeshes
void MapCanvas::forceUpdateMeshes()
{
    if (m_batches.remeshCookie.isPending()) {
        update();
        return;
    }
    updateVisibleRoomAreas();
    std::vector<RoomArea> allVisibleRoomAreas(m_visibleRoomAreas.begin(), m_visibleRoomAreas.end());
    if (!allVisibleRoomAreas.empty()) {
        for(const auto& roomArea : allVisibleRoomAreas) {
            m_pendingRoomAreaGenerations.insert(roomArea);
        }
        m_batches.remeshCookie.set(
            m_data.generateRoomAreaBatches(mctp::getProxy(m_textures), allVisibleRoomAreas)
        );
        m_cookieAssociatedAreas = allVisibleRoomAreas;
    }
    m_diff.resetExistingMeshesAndIgnorePendingRemesh();
    update();
}

// This is the first, correct definition of slot_mapChanged
void MapCanvas::slot_mapChanged()
{
    if ((false)) { // This ((false)) is suspicious, but part of original code logic
        m_batches.mapBatches.reset();
    }
    update();
}

// This is the first, correct definition of slot_requestUpdate
void MapCanvas::slot_requestUpdate()
{
    update();
}

// This is the first, correct definition of screenChanged
void MapCanvas::screenChanged()
{
    auto &gl = getOpenGL();
    if (!gl.isRendererInitialized()) return;
    const auto newDpi = static_cast<float>(QPaintDevice::devicePixelRatioF());
    const auto oldDpi = gl.getDevicePixelRatio();
    if (!utils::equals(newDpi, oldDpi)) {
        log(QString("Display: %1 DPI").arg(static_cast<double>(newDpi)));
        m_batches.resetExistingMeshesButKeepPendingRemesh();
        gl.setDevicePixelRatio(newDpi);
        auto &font = getGLFont();
        font.cleanup();
        font.init();
        forceUpdateMeshes();
    }
}

// This is the first, correct definition of selectionChanged
void MapCanvas::selectionChanged()
{
    update();
    emit sig_selectionChanged();
}

// This is the first, correct definition of graphicsSettingsChanged
void MapCanvas::graphicsSettingsChanged()
{
    update();
}

// This is the first, correct definition of userPressedEscape
void MapCanvas::userPressedEscape(bool /*pressed*/)
{
    switch (m_canvasMouseMode) {
    case CanvasMouseModeEnum::NONE:
    case CanvasMouseModeEnum::CREATE_ROOMS:
        break;
    case CanvasMouseModeEnum::CREATE_CONNECTIONS:
    case CanvasMouseModeEnum::SELECT_CONNECTIONS:
    case CanvasMouseModeEnum::CREATE_ONEWAY_CONNECTIONS:
        slot_clearConnectionSelection();
        break;
    case CanvasMouseModeEnum::RAYPICK_ROOMS:
    case CanvasMouseModeEnum::SELECT_ROOMS:
        m_selectedArea = false;
        m_roomSelectionMove.reset();
        slot_clearRoomSelection();
        break;
    case CanvasMouseModeEnum::MOVE:
        FALLTHROUGH;
    case CanvasMouseModeEnum::SELECT_INFOMARKS:
    case CanvasMouseModeEnum::CREATE_INFOMARKS:
        m_infoMarkSelectionMove.reset();
        slot_clearInfoMarkSelection();
        break;
    }
}

// All subsequent function definitions are duplicates and will be removed by the overwrite.
// For example, the next line in the actual file might be:
// std::optional<glm::vec3> MapCanvas::getUnprojectedScreenPos(const glm::vec2& screenPos) const { ... } // Duplicate
// And so on for all other methods listed in the subtask.```cpp
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
#include "../map/coordinate.h" // For Bounds
#include "../map/exit.h"
#include "../map/infomark.h"
#include "../map/room.h"
#include "../map/roomid.h"
#include "../mapdata/mapdata.h"
#include "../mapdata/roomselection.h"
#include "InfoMarkSelection.h"
#include "MapCanvasData.h"
#include "MapCanvasRoomDrawer.h" // For Batches
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

    // Provide MapData with a pointer to MapCanvas's batches
    m_data.setMapCanvasBatches(m_batches.mapBatches ? &(*m_batches.mapBatches) : nullptr);
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
            if (event->angleDelta().y() > 100) slot_layerDown();
            else if (event->angleDelta().y() < -100) slot_layerUp();
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

bool MapCanvas::event(QEvent *const event)
{
    auto tryHandlePinchZoom = [this, event]() -> bool {
        if (event->type() != QEvent::Gesture) return false;
        const auto *const gestureEvent = dynamic_cast<QGestureEvent *>(event);
        if (!gestureEvent) return false;
        QGesture *const gesture = gestureEvent->gesture(Qt::PinchGesture);
        const auto *const pinch = dynamic_cast<QPinchGesture *>(gesture);
        if (!pinch) return false;
        const QPinchGesture::ChangeFlags changeFlags = pinch->changeFlags();
        if (changeFlags & QPinchGesture::ScaleFactorChanged) {
            m_scaleFactor.setPinch(static_cast<float>(pinch->totalScaleFactor()));
        }
        if (pinch->state() == Qt::GestureFinished) {
            m_scaleFactor.endPinch();
            zoomChanged();
        }
        resizeGL();
        return true;
    };
    if (tryHandlePinchZoom()) return true;
    return QOpenGLWidget::event(event);
}

void MapCanvas::slot_createRoom()
{
    if (!hasSel1()) return;
    const Coordinate c = getSel1().getCoordinate();
    if (m_data.findRoomHandle(c)) return;
    m_data.createEmptyRoom(Coordinate{c.x, c.y, m_currentLayer});
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
        return InfoMarkSelection::alloc(m_data, pos + Coordinate{-INFOMARK_CLICK_RADIUS, -INFOMARK_CLICK_RADIUS, 0}, pos + Coordinate{+INFOMARK_CLICK_RADIUS, +INFOMARK_CLICK_RADIUS, 0});
    }
    const glm::vec2 clickPoint = glm::vec2{optClickPoint.value()};
    glm::vec3 maxCoord = center;
    glm::vec3 minCoord = center;
    const auto probe = [&](const glm::vec2 &offset){ const auto coord = unproject_clamped(clickPoint + offset); maxCoord = glm::max(maxCoord, coord); minCoord = glm::min(minCoord, coord); };
    for (int dy = -1; dy <= 1; ++dy) for (int dx = -1; dx <= 1; ++dx) probe(glm::vec2{static_cast<float>(dx) * CLICK_RADIUS, static_cast<float>(dy) * CLICK_RADIUS});
    const auto getScaled = [this](const glm::vec3 &c){ const auto pos = glm::ivec3(glm::vec2(c) * static_cast<float>(INFOMARK_SCALE), m_currentLayer); return Coordinate{pos.x, pos.y, pos.z}; };
    return InfoMarkSelection::alloc(m_data, getScaled(minCoord), getScaled(maxCoord));
}

void MapCanvas::mousePressEvent(QMouseEvent *const event) {
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
    case CanvasMouseModeEnum::CREATE_INFOMARKS: infomarksChanged(); break;
    case CanvasMouseModeEnum::SELECT_INFOMARKS:
        if (hasLeftButton && hasSel1()) {
            auto tmpSel = getInfoMarkSelection(getSel1());
            if (m_infoMarkSelection && !tmpSel->empty() && m_infoMarkSelection->contains(tmpSel->front().getId())) {
                m_infoMarkSelectionMove.emplace();
            } else {
                m_selectedArea = false; m_infoMarkSelectionMove.reset();
            }
        }
        selectionChanged(); break;
    case CanvasMouseModeEnum::MOVE: if (hasLeftButton && hasSel1()) { setCursor(Qt::ClosedHandCursor); startMoving(m_sel1.value()); } break;
    case CanvasMouseModeEnum::RAYPICK_ROOMS:
        if (hasLeftButton) {
            const auto xy = getMouseCoords(event); const auto near_p = unproject_raw(glm::vec3{xy,0.f}); const auto far_p = unproject_raw(glm::vec3{xy,1.f});
            const auto hiz = static_cast<int>(std::floor(near_p.z)); const auto loz = static_cast<int>(std::ceil(far_p.z));
            if (hiz <= loz) break;
            const auto inv_denom = 1.f / (far_p.z - near_p.z);
            RoomIdSet tmpSel;
            for (int z = hiz; z >= loz; --z) {
                const float t = (static_cast<float>(z)-near_p.z)*inv_denom; assert(isClamped(t,0.f,1.f)); const auto pos_mix = glm::mix(near_p,far_p,t);
                assert(static_cast<int>(std::lround(pos_mix.z))==z);
                if(const auto r = m_data.findRoomHandle(MouseSel{Coordinate2f{pos_mix.x,pos_mix.y},z}.getCoordinate())) tmpSel.insert(r.getId());
            }
            if(!tmpSel.empty()) slot_setRoomSelection(SigRoomSelection(RoomSelection::createSelection(std::move(tmpSel))));
            selectionChanged();
        } break;
    case CanvasMouseModeEnum::SELECT_ROOMS:
        if (hasRightButton) { m_selectedArea = false; slot_clearRoomSelection(); }
        if (hasLeftButton && hasSel1()) {
            if (!hasCtrl) {
                const auto pRoom = m_data.findRoomHandle(getSel1().getCoordinate());
                if (pRoom.exists() && m_roomSelection && m_roomSelection->contains(pRoom.getId())) m_roomSelectionMove.emplace(RoomSelMove{});
                else { m_roomSelectionMove.reset(); m_selectedArea = false; slot_clearRoomSelection(); }
            } else m_ctrlPressed = true;
        }
        selectionChanged(); break;
    case CanvasMouseModeEnum::CREATE_ONEWAY_CONNECTIONS: case CanvasMouseModeEnum::CREATE_CONNECTIONS:
        if (hasLeftButton && hasSel1()) { m_connectionSelection = ConnectionSelection::alloc(m_data, getSel1()); if(!m_connectionSelection->isFirstValid()) m_connectionSelection=nullptr; emit sig_newConnectionSelection(nullptr); }
        if (hasRightButton) slot_clearConnectionSelection();
        selectionChanged(); break;
    case CanvasMouseModeEnum::SELECT_CONNECTIONS:
        if (hasLeftButton && hasSel1()) {
            m_connectionSelection = ConnectionSelection::alloc(m_data, getSel1());
            if (!m_connectionSelection->isFirstValid() || m_connectionSelection->getFirst().room.getExit(m_connectionSelection->getFirst().direction).outIsEmpty()) m_connectionSelection = nullptr;
            emit sig_newConnectionSelection(nullptr);
        }
        if (hasRightButton) slot_clearConnectionSelection();
        selectionChanged(); break;
    case CanvasMouseModeEnum::CREATE_ROOMS: slot_createRoom(); break;
    case CanvasMouseModeEnum::NONE: default: break;
    }
}
void MapCanvas::mouseMoveEvent(QMouseEvent *const event) {
    const bool hasLeftButton = (event->buttons() & Qt::LeftButton) != 0u;
    if(m_canvasMouseMode != CanvasMouseModeEnum::MOVE){
        const int vS = [this,event](){int h=height(),M=std::min(100,h/4),y=event->pos().y(); if(y<M)return SCROLL_SCALE; else if(y>h-M)return -SCROLL_SCALE; else return 0;}();
        const int hS = [this,event](){int w=width(),M=std::min(100,w/4),x=event->pos().x(); if(x<M)return -SCROLL_SCALE; else if(x>w-M)return SCROLL_SCALE; else return 0;}();
        emit sig_continuousScroll(hS,vS);
    }
    m_sel2=getUnprojectedMouseSel(event);
    switch(m_canvasMouseMode){
    case CanvasMouseModeEnum::SELECT_INFOMARKS: if(hasLeftButton&&hasSel1()&&hasSel2()){if(hasInfoMarkSelectionMove()){m_infoMarkSelectionMove->pos=getSel2().pos-getSel1().pos;setCursor(Qt::ClosedHandCursor);}else m_selectedArea=true;} selectionChanged(); break;
    case CanvasMouseModeEnum::CREATE_INFOMARKS: if(hasLeftButton)m_selectedArea=true; infomarksChanged(); break;
    case CanvasMouseModeEnum::MOVE: if(hasLeftButton&&m_mouseLeftPressed&&hasSel2()&&hasBackup()){const Coordinate2i d=((getSel2().pos-getBackup().pos)*static_cast<float>(SCROLL_SCALE)).truncate(); if(d.x!=0||d.y!=0)emit sig_mapMove(-d.x,-d.y);} break;
    case CanvasMouseModeEnum::RAYPICK_ROOMS: break;
    case CanvasMouseModeEnum::SELECT_ROOMS: if(hasLeftButton){if(hasRoomSelectionMove()&&hasSel1()&&hasSel2()){auto&m=this->m_data; auto iM=[&m](RoomSelection&s,const Coordinate&o){s.removeMissing(m);return m.getCurrentMap().wouldAllowRelativeMove(s.getRoomIds(),o);}; const auto df=getSel2().pos.truncate()-getSel1().pos.truncate(); const auto wP=!iM(deref(m_roomSelection),Coordinate{df,0}); m_roomSelectionMove->pos=df;m_roomSelectionMove->wrongPlace=wP;setCursor(wP?Qt::ForbiddenCursor:Qt::ClosedHandCursor);}else m_selectedArea=true;} selectionChanged(); break;
    case CanvasMouseModeEnum::CREATE_ONEWAY_CONNECTIONS: case CanvasMouseModeEnum::CREATE_CONNECTIONS: if(hasLeftButton&&hasSel2()){if(m_connectionSelection){m_connectionSelection->setSecond(getSel2());if(m_connectionSelection->isValid()&&!m_connectionSelection->isCompleteNew())m_connectionSelection->removeSecond();selectionChanged();}}else slot_requestUpdate(); break;
    case CanvasMouseModeEnum::SELECT_CONNECTIONS: if(hasLeftButton&&hasSel2()){if(m_connectionSelection){m_connectionSelection->setSecond(getSel2());if(!m_connectionSelection->isCompleteExisting())m_connectionSelection->removeSecond();selectionChanged();}} break;
    case CanvasMouseModeEnum::CREATE_ROOMS: case CanvasMouseModeEnum::NONE: default: break;
    }
}
void MapCanvas::mouseReleaseEvent(QMouseEvent *const event) {
    emit sig_continuousScroll(0,0); m_sel2=getUnprojectedMouseSel(event); if(m_mouseRightPressed)m_mouseRightPressed=false;
    switch(m_canvasMouseMode){
    case CanvasMouseModeEnum::SELECT_INFOMARKS: setCursor(Qt::ArrowCursor); if(m_mouseLeftPressed){m_mouseLeftPressed=false; if(hasInfoMarkSelectionMove()){const auto p=m_infoMarkSelectionMove->pos;m_infoMarkSelectionMove.reset(); if(m_infoMarkSelection){const auto o=Coordinate{(p*INFOMARK_SCALE).truncate(),0};const InfoMarkSelection&s=deref(m_infoMarkSelection);s.applyOffset(o);infomarksChanged();}}else if(hasSel1()&&hasSel2()){const auto c1=getSel1().getScaledCoordinate(INFOMARK_SCALE);const auto c2=getSel2().getScaledCoordinate(INFOMARK_SCALE); auto tS=InfoMarkSelection::alloc(m_data,c1,c2); if(tS&&tS->size()==1){const auto&fM=tS->front();const auto&p=fM.getPosition1();log(QString("Selected Info Mark: [%1] (at %2,%3,%4)").arg(fM.getText().toQString()).arg(p.x).arg(p.y).arg(p.z));} slot_setInfoMarkSelection(tS);} m_selectedArea=false;} selectionChanged(); break;
    case CanvasMouseModeEnum::CREATE_INFOMARKS: if(m_mouseLeftPressed&&hasSel1()&&hasSel2()){m_mouseLeftPressed=false;const auto c1=getSel1().getScaledCoordinate(INFOMARK_SCALE);const auto c2=getSel2().getScaledCoordinate(INFOMARK_SCALE);slot_setInfoMarkSelection(InfoMarkSelection::allocEmpty(m_data,c1,c2));} infomarksChanged(); break;
    case CanvasMouseModeEnum::MOVE: stopMoving();setCursor(Qt::OpenHandCursor);if(m_mouseLeftPressed)m_mouseLeftPressed=false; if(hasSel1()&&hasSel2()&&getSel1().to_vec3()==getSel2().to_vec3()){if(const auto r=m_data.findRoomHandle(getSel1().getCoordinate())){auto msg=mmqt::previewRoom(r,mmqt::StripAnsiEnum::Yes,mmqt::PreviewStyleEnum::ForDisplay);QToolTip::showText(mapToGlobal(event->pos()),msg,this,rect(),5000);}} break;
    case CanvasMouseModeEnum::RAYPICK_ROOMS: break;
    case CanvasMouseModeEnum::SELECT_ROOMS: setCursor(Qt::ArrowCursor); if(m_ctrlPressed&&m_altPressed)break; if(m_mouseLeftPressed){m_mouseLeftPressed=false;if(m_roomSelectionMove.has_value()){const auto p=m_roomSelectionMove->pos;const bool wP=m_roomSelectionMove->wrongPlace;m_roomSelectionMove.reset();if(!wP&&(m_roomSelection))m_data.applySingleChange(Change{room_change_types::MoveRelative2{m_roomSelection->getRoomIds(),Coordinate{p,0}}});}else{if(hasSel1()&&hasSel2()){const auto&c1=getSel1().getCoordinate();const auto&c2=getSel2().getCoordinate();if(m_roomSelection==nullptr)m_roomSelection=RoomSelection::createSelection(m_data.findAllRooms(c1,c2));else{auto&s=deref(m_roomSelection);s.removeMissing(m_data);const auto tS=m_data.findAllRooms(c1,c2);for(const RoomId k:tS){if(s.contains(k))s.erase(k);else s.insert(k);}}}}if(m_roomSelection&&!m_roomSelection->empty())slot_setRoomSelection(SigRoomSelection{m_roomSelection});}m_selectedArea=false;}selectionChanged();break;
    case CanvasMouseModeEnum::CREATE_ONEWAY_CONNECTIONS: case CanvasMouseModeEnum::CREATE_CONNECTIONS: if(m_mouseLeftPressed&&hasSel1()&&hasSel2()){m_mouseLeftPressed=false;if(m_connectionSelection==nullptr)m_connectionSelection=ConnectionSelection::alloc(m_data,getSel1());m_connectionSelection->setSecond(getSel2());if(!m_connectionSelection->isValid())m_connectionSelection=nullptr;else{const auto f=m_connectionSelection->getFirst();const auto s=m_connectionSelection->getSecond();const auto&r1=f.room;const auto&r2=s.room;if(r1.exists()&&r2.exists()){const auto d1=f.direction;const auto d2=s.direction;const RoomId id1=r1.getId();const RoomId id2=r2.getId();const bool iCN=m_connectionSelection->isCompleteNew();m_connectionSelection=nullptr;if(iCN){const WaysEnum w=(m_canvasMouseMode!=CanvasMouseModeEnum::CREATE_ONEWAY_CONNECTIONS)?WaysEnum::TwoWay:WaysEnum::OneWay;m_data.applySingleChange(Change{exit_change_types::ModifyExitConnection{ChangeTypeEnum::Add,id1,d1,id2,w}});}m_connectionSelection=ConnectionSelection::alloc(m_data);m_connectionSelection->setFirst(id1,d1);m_connectionSelection->setSecond(id2,d2);slot_mapChanged();}}}slot_setConnectionSelection(m_connectionSelection);}selectionChanged();break;
    case CanvasMouseModeEnum::SELECT_CONNECTIONS: if(m_mouseLeftPressed&&(m_connectionSelection||hasSel1())&&hasSel2()){m_mouseLeftPressed=false;if(m_connectionSelection==nullptr&&hasSel1())m_connectionSelection=ConnectionSelection::alloc(m_data,getSel1());m_connectionSelection->setSecond(getSel2());if(!m_connectionSelection->isValid()||!m_connectionSelection->isCompleteExisting())m_connectionSelection=nullptr;slot_setConnectionSelection(m_connectionSelection);}selectionChanged();break;
    case CanvasMouseModeEnum::CREATE_ROOMS: if(m_mouseLeftPressed)m_mouseLeftPressed=false; break;
    case CanvasMouseModeEnum::NONE: default: break;
    }
    m_altPressed=false;m_ctrlPressed=false;
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
void MapCanvas::layerChanged() { updateVisibleRoomAreas(); requestMissingRoomAreas(); update(); }

void MapCanvas::requestMissingRoomAreas() {
    if (m_batches.remeshCookie.isPending()) return;
    std::vector<RoomArea> roomAreasToRequestNow;
    for (const RoomArea& roomArea : m_visibleRoomAreas) {
        bool isLoaded = m_data.isRoomAreaLoaded(roomArea);
        if (!isLoaded) {
            if (m_pendingRoomAreaGenerations.find(roomArea) == m_pendingRoomAreaGenerations.end()) {
                roomAreasToRequestNow.push_back(roomArea);
            }
        }
    }
    if (!roomAreasToRequestNow.empty()) {
        for(const RoomArea& roomArea : roomAreasToRequestNow) m_pendingRoomAreaGenerations.insert(roomArea);
        m_batches.remeshCookie.set(m_data.generateRoomAreaBatches(mctp::getProxy(m_textures), roomAreasToRequestNow));
        m_cookieAssociatedAreas = roomAreasToRequestNow;
    }
}

std::optional<glm::vec3> MapCanvas::getUnprojectedScreenPos(const glm::vec2& screenPos) const {
    return this->unproject_clamped(screenPos);
}

void MapCanvas::updateVisibleRoomAreas() {
    m_visibleRoomAreas.clear();
    const auto tl_opt = getUnprojectedScreenPos(glm::vec2(0.0f, 0.0f));
    const auto br_opt = getUnprojectedScreenPos(glm::vec2(static_cast<float>(width()), static_cast<float>(height())));
    if (!tl_opt || !br_opt) return;
    const glm::vec2 worldTopLeft(std::floor(tl_opt->x), std::floor(tl_opt->y));
    const glm::vec2 worldBottomRight(std::floor(br_opt->x), std::floor(br_opt->y));
    const float minX = worldTopLeft.x;
    const float maxX = worldBottomRight.x;
    const float minY = worldTopLeft.y;
    const float maxY = worldBottomRight.y;
    const int minZ = m_currentLayer - getConfig().canvas.mapRadius[2];
    const int maxZ = m_currentLayer + getConfig().canvas.mapRadius[2];

    for (int z = minZ; z <= maxZ; ++z) {
        Coordinate areaMinCoord(static_cast<int>(std::min(minX, maxX)), static_cast<int>(std::min(minY, maxY)), z);
        Coordinate areaMaxCoord(static_cast<int>(std::max(minX, maxX)), static_cast<int>(std::max(minY, maxY)), z);
        RoomIdSet roomIdsInRect = m_data.getRoomIdsInBoundingBox(Bounds{areaMinCoord, areaMaxCoord}, z);
        for (RoomId roomId : roomIdsInRect) {
            if (auto roomHandle = m_data.findRoomHandle(roomId)) {
                m_visibleRoomAreas.insert(roomHandle.getArea());
            }
        }
    }
}

void MapCanvas::forceUpdateMeshes()
{
    if (m_batches.remeshCookie.isPending()) {
        update();
        return;
    }
    updateVisibleRoomAreas();
    std::vector<RoomArea> allVisibleRoomAreas(m_visibleRoomAreas.begin(), m_visibleRoomAreas.end());
    if (!allVisibleRoomAreas.empty()) {
        for(const auto& roomArea : allVisibleRoomAreas) {
            m_pendingRoomAreaGenerations.insert(roomArea);
        }
        m_batches.remeshCookie.set(
            m_data.generateRoomAreaBatches(mctp::getProxy(m_textures), allVisibleRoomAreas)
        );
        m_cookieAssociatedAreas = allVisibleRoomAreas;
    }
    m_diff.resetExistingMeshesAndIgnorePendingRemesh();
    update();
}

void MapCanvas::slot_mapChanged()
{
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
    if (!gl.isRendererInitialized()) return;
    const auto newDpi = static_cast<float>(QPaintDevice::devicePixelRatioF());
    const auto oldDpi = gl.getDevicePixelRatio();
    if (!utils::equals(newDpi, oldDpi)) {
        log(QString("Display: %1 DPI").arg(static_cast<double>(newDpi)));
        m_batches.resetExistingMeshesButKeepPendingRemesh();
        gl.setDevicePixelRatio(newDpi);
        auto &font = getGLFont();
        font.cleanup();
        font.init();
        forceUpdateMeshes();
    }
}

void MapCanvas::selectionChanged()
{
    update();
    emit sig_selectionChanged();
}

void MapCanvas::graphicsSettingsChanged()
{
    update();
}

void MapCanvas::userPressedEscape(bool /*pressed*/)
{
    switch (m_canvasMouseMode) {
    case CanvasMouseModeEnum::NONE:
    case CanvasMouseModeEnum::CREATE_ROOMS:
        break;
    case CanvasMouseModeEnum::CREATE_CONNECTIONS:
    case CanvasMouseModeEnum::SELECT_CONNECTIONS:
    case CanvasMouseModeEnum::CREATE_ONEWAY_CONNECTIONS:
        slot_clearConnectionSelection();
        break;
    case CanvasMouseModeEnum::RAYPICK_ROOMS:
    case CanvasMouseModeEnum::SELECT_ROOMS:
        m_selectedArea = false;
        m_roomSelectionMove.reset();
        slot_clearRoomSelection();
        break;
    case CanvasMouseModeEnum::MOVE:
        FALLTHROUGH;
    case CanvasMouseModeEnum::SELECT_INFOMARKS:
    case CanvasMouseModeEnum::CREATE_INFOMARKS:
        m_infoMarkSelectionMove.reset();
        slot_clearInfoMarkSelection();
        break;
    }
}

// End of the first block of correct function definitions.
// Subsequent text in the original file (from the user's description)
// would be duplicates and are removed in this overwrite.
```
