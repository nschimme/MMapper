#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#include "../global/macros.h"
#include "../mapdata/roomselection.h"
#include "CanvasMouseModeEnum.h"
#include "connectionselection.h"
#include "InfomarkSelection.h"
#include "MapCanvasData.h"
#include "MapCanvasRoomDrawer.h"
#include <QObject>
#include <glm/vec2.hpp>
#include <memory>
#include <optional>
#include <future>

class MapData;
class PrespammedPath;
class QInputEvent;
class QMouseEvent;
class QWheelEvent;

class NODISCARD_QOBJECT MapCanvasViewModel final : public QObject, public MapCanvasViewport, public MapCanvasInputState
{
    Q_OBJECT
    Q_PROPERTY(float zoom READ zoom WRITE setZoom NOTIFY zoomChanged)
    Q_PROPERTY(int layer READ layer WRITE setLayer NOTIFY layerChanged)
    Q_PROPERTY(CanvasMouseModeEnum mouseMode READ mouseMode WRITE setMouseMode NOTIFY mouseModeChanged)

public:
    explicit MapCanvasViewModel(MapData &mapData, PrespammedPath &prespammedPath, QObject *parent = nullptr);

    NODISCARD float zoom() const { return m_scaleFactor.getRaw(); }
    void setZoom(float z);
    NODISCARD int layer() const { return m_currentLayer; }
    void setLayer(int l);
    NODISCARD CanvasMouseModeEnum mouseMode() const { return m_canvasMouseMode; }
    void setMouseMode(CanvasMouseModeEnum m);

    void zoomIn();
    void zoomOut();
    void zoomReset();
    void layerUp();
    void layerDown();
    void layerReset();

    void handleMousePress(QMouseEvent *e);
    void handleMouseMove(QMouseEvent *e);
    void handleMouseRelease(QMouseEvent *e);
    void handleWheel(QWheelEvent *e);
    bool handleEvent(QEvent *e);

    void setViewportRect(const Viewport &vp) { m_viewportRect = vp; }

    // Selection helpers
    void clearRoomSelection();
    void clearConnectionSelection();
    void clearInfomarkSelection();
    void clearAllSelections();

signals:
    void zoomChanged();
    void layerChanged();
    void mouseModeChanged();
    void selectionChanged();
    void sig_requestUpdate();
    void sig_mapMove(int dx, int dy);
    void sig_center(const glm::vec2 &worldCoord);
    void sig_newRoomSelection(const SigRoomSelection &s);
    void sig_newConnectionSelection(ConnectionSelection *s);
    void sig_newInfomarkSelection(InfomarkSelection *s);
    void sig_setCursor(Qt::CursorShape shape);
    void sig_continuousScroll(int dx, int dy);
    void sig_log(const QString &category, const QString &message);

private:
    std::shared_ptr<InfomarkSelection> getInfomarkSelection(const MouseSel &sel);
    MapData &m_mapData;
    struct AltDragState { QPoint lastPos; Qt::CursorShape originalCursor; };
    std::optional<AltDragState> m_altDragState;
};
