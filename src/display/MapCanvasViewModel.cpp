// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#include "MapCanvasViewModel.h"
#include "../mapdata/mapdata.h"
#include "prespammedpath.h"
#include <QMouseEvent>
#include <QWheelEvent>

MapCanvasViewModel::MapCanvasViewModel(MapData &mapData, PrespammedPath &prespammedPath, QObject *parent)
    : QObject(parent)
    , MapCanvasInputState(prespammedPath)
    , m_mapData(mapData)
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
        emit mouseModeChanged();
    }
}

void MapCanvasViewModel::handleMousePress(QMouseEvent *e)
{
    m_sel1 = m_sel2 = getUnprojectedMouseSel(e);
    if (e->button() == Qt::LeftButton) m_mouseLeftPressed = true;
    if (e->button() == Qt::RightButton) m_mouseRightPressed = true;

    if (m_canvasMouseMode == CanvasMouseModeEnum::MOVE && m_mouseLeftPressed) {
        // Start move
    }
    emit sig_requestUpdate();
}

void MapCanvasViewModel::handleMouseMove(QMouseEvent *e)
{
    m_sel2 = getUnprojectedMouseSel(e);
    if (m_mouseLeftPressed && m_canvasMouseMode == CanvasMouseModeEnum::MOVE) {
        if (m_sel1.has_value()) {
            // Calculate delta and signal map move
        }
    }
    emit sig_requestUpdate();
}

void MapCanvasViewModel::handleMouseRelease(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton) {
        m_mouseLeftPressed = false;
        if (m_canvasMouseMode == CanvasMouseModeEnum::SELECT_ROOMS) {
            // Signal room selection
        }
    }
    if (e->button() == Qt::RightButton) m_mouseRightPressed = false;
    emit sig_requestUpdate();
}

void MapCanvasViewModel::handleWheel(QWheelEvent *e)
{
    if (e->angleDelta().y() > 0) zoomIn();
    else zoomOut();
}

bool MapCanvasViewModel::handleEvent(QEvent *e)
{
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
