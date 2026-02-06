// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#include "MapCanvasViewModel.h"
#include "../mapdata/mapdata.h"
#include "../global/parserutils.h"
#include "prespammedpath.h"
#include "../group/mmapper2group.h"
#include <QMouseEvent>
#include <QWheelEvent>
#include <QDebug>

MapCanvasViewModel::MapCanvasViewModel(MapData &mapData, PrespammedPath &prespammedPath, Mmapper2Group &groupManager, QObject *parent)
    : QObject(parent)
    , MapCanvasInputState(prespammedPath)
    , m_mapData(mapData)
    , m_groupManager(groupManager)
{
}

void MapCanvasViewModel::setZoom(float z)
{
    if (m_scaleFactor.getRaw() != z) {
        m_scaleFactor.set(z);
        emit zoomChanged();
        emit sig_requestUpdate();
    }
}

void MapCanvasViewModel::setLayer(int l)
{
    if (m_currentLayer != l) {
        m_currentLayer = l;
        emit layerChanged();
        emit sig_requestUpdate();
    }
}

void MapCanvasViewModel::setMouseMode(CanvasMouseModeEnum m)
{
    if (m_canvasMouseMode != m) {
        m_canvasMouseMode = m;
        m_selectedArea = false;

        Qt::CursorShape cursor = Qt::ArrowCursor;
        switch (m) {
            case CanvasMouseModeEnum::MOVE: cursor = Qt::OpenHandCursor; break;
            case CanvasMouseModeEnum::RAYPICK_ROOMS:
            case CanvasMouseModeEnum::SELECT_CONNECTIONS:
            case CanvasMouseModeEnum::CREATE_INFOMARKS: cursor = Qt::CrossCursor; break;
            default: cursor = Qt::ArrowCursor; break;
        }
        emit sig_setCursor(cursor);
        emit mouseModeChanged();
        emit selectionChanged();
        emit sig_requestUpdate();
    }
}

void MapCanvasViewModel::handleMousePress(QMouseEvent *e)
{
    auto sel = getUnprojectedMouseSel(e);
    m_sel1 = m_sel2 = sel;

    if (e->button() == Qt::LeftButton) {
        m_mouseLeftPressed = true;
        if (m_canvasMouseMode == CanvasMouseModeEnum::MOVE) {
            emit sig_setCursor(Qt::ClosedHandCursor);
        }
    }
    if (e->button() == Qt::RightButton) {
        m_mouseRightPressed = true;
        if (e->modifiers() & Qt::AltModifier) {
            m_altDragState = {e->pos(), Qt::ArrowCursor}; // simplified
        }
    }

    emit sig_requestUpdate();
}

void MapCanvasViewModel::handleMouseMove(QMouseEvent *e)
{
    m_sel2 = getUnprojectedMouseSel(e);

    if (m_mouseLeftPressed) {
        if (m_canvasMouseMode == CanvasMouseModeEnum::MOVE) {
            // Simplified movement logic
            if (m_sel1 && m_sel2) {
                // Actually movement is usually done via delta in screen coordinates for smoother panning
            }
        }
    }

    emit sig_requestUpdate();
}

void MapCanvasViewModel::handleMouseRelease(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton) {
        m_mouseLeftPressed = false;
        if (m_canvasMouseMode == CanvasMouseModeEnum::MOVE) {
            emit sig_setCursor(Qt::OpenHandCursor);
        } else if (m_canvasMouseMode == CanvasMouseModeEnum::SELECT_ROOMS) {
            if (m_sel1 && m_sel2) {
                auto hi = Coordinate::max(m_sel1->getCoordinate(), m_sel2->getCoordinate());
                auto lo = Coordinate::min(m_sel1->getCoordinate(), m_sel2->getCoordinate());
                m_roomSelection = RoomSelection::alloc(m_mapData, lo, hi);
                emit sig_newRoomSelection(SigRoomSelection(m_roomSelection));
                emit selectionChanged();
            }
        }
    }
    if (e->button() == Qt::RightButton) {
        m_mouseRightPressed = false;
        if (m_altDragState) {
            m_altDragState.reset();
        } else {
            emit sig_showContextMenu(e->pos());
        }
    }
    emit sig_requestUpdate();
}

void MapCanvasViewModel::handleWheel(QWheelEvent *e)
{
    if (e->angleDelta().y() > 0) zoomIn();
    else zoomOut();
}

bool MapCanvasViewModel::handleEvent(QEvent *e)
{
    // Handle gestures etc here
    return false;
}

void MapCanvasViewModel::zoomIn() { m_scaleFactor.logStep(1); emit zoomChanged(); emit sig_requestUpdate(); }
void MapCanvasViewModel::zoomOut() { m_scaleFactor.logStep(-1); emit zoomChanged(); emit sig_requestUpdate(); }
void MapCanvasViewModel::zoomReset() { m_scaleFactor.reset(); emit zoomChanged(); emit sig_requestUpdate(); }
void MapCanvasViewModel::layerUp() { setLayer(m_currentLayer + 1); }
void MapCanvasViewModel::layerDown() { setLayer(m_currentLayer - 1); }
void MapCanvasViewModel::layerReset() { setLayer(0); }

void MapCanvasViewModel::clearRoomSelection() { m_roomSelection.reset(); emit selectionChanged(); emit sig_requestUpdate(); }
void MapCanvasViewModel::clearConnectionSelection() { m_connectionSelection.reset(); emit selectionChanged(); emit sig_requestUpdate(); }
void MapCanvasViewModel::clearInfomarkSelection() { m_infoMarkSelection.reset(); emit selectionChanged(); emit sig_requestUpdate(); }
void MapCanvasViewModel::clearAllSelections() { clearRoomSelection(); clearConnectionSelection(); clearInfomarkSelection(); }

void MapCanvasViewModel::createRoom()
{
    if (!m_sel1) return;
    const Coordinate c = m_sel1->getCoordinate();
    if (m_mapData.findRoomHandle(c)) return;
    m_mapData.createEmptyRoom(Coordinate{c.x, c.y, m_currentLayer});
    emit sig_requestUpdate();
}

std::shared_ptr<InfomarkSelection> MapCanvasViewModel::getInfomarkSelection(const MouseSel &sel)
{
    // Ported from MapCanvas::getInfomarkSelection
    static constexpr float CLICK_RADIUS = 10.f;
    const auto center = sel.to_vec3();
    const auto optClickPoint = project(center);
    if (!optClickPoint) return nullptr;

    const glm::vec2 clickPoint = glm::vec2{optClickPoint.value()};
    glm::vec3 maxCoord = center;
    glm::vec3 minCoord = center;

    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            const auto coord = unproject_clamped(clickPoint + glm::vec2{static_cast<float>(dx) * CLICK_RADIUS, static_cast<float>(dy) * CLICK_RADIUS});
            maxCoord = glm::max(maxCoord, coord);
            minCoord = glm::min(minCoord, coord);
        }
    }

    const auto getScaled = [this](const glm::vec3 &c) -> Coordinate {
        return Coordinate{static_cast<int>(c.x * INFOMARK_SCALE), static_cast<int>(c.y * INFOMARK_SCALE), m_currentLayer};
    };
    return InfomarkSelection::alloc(m_mapData, getScaled(minCoord), getScaled(maxCoord));
}
