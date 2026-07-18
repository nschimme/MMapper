#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Ulf Hermann <ulfonk_mennhar@gmx.de> (Alve)
// Author: Marek Krejza <krejza@gmail.com> (Caligor)
// Author: Nils Schimmelmann <nschimme@gmail.com> (Jahara)

// MapCanvas is a thin QOpenGLWindow shell around MapCanvasCore, which holds
// all of the GL/paint/input/signal machinery. This split lets a future
// QQuickFramebufferObject-based item drive the same MapCanvasCore without
// depending on QOpenGLWindow (or QtWidgets) at all.
//
// MapCanvas re-exposes MapCanvasCore's public signal/slot surface by
// forwarding (rather than by exposing a getCore() accessor) so that existing
// callers (MainWindow, MapWindow, ...) don't need to change at all -- they
// keep connecting to `MapCanvas::sig_*`/`MapCanvas::slot_*` exactly as before.

#include "../global/Signal2.h"
#include "../map/PromptFlags.h"
#include "../mapdata/roomselection.h"
#include "CanvasMouseModeEnum.h"
#include "MapCanvasCore.h"

#include <memory>

#include <QOpenGLDebugMessage>
#include <QOpenGLWindow>
#include <QtCore>

class ConnectionSelection;
class Coordinate;
class GameObserver;
class InfomarkSelection;
class MapData;
class Mmapper2Group;
class PrespammedPath;
class QMouseEvent;
class QNativeGestureEvent;
class QTouchEvent;
class QWheelEvent;

class NODISCARD_QOBJECT MapCanvas final : public QOpenGLWindow, private ICanvasHost
{
    Q_OBJECT

public:
    static constexpr const int SCROLL_SCALE = MapCanvasCore::SCROLL_SCALE;

private:
    MapCanvasCore m_core;

public:
    explicit MapCanvas(MapData &mapData,
                       GameObserver &observer,
                       PrespammedPath &prespammedPath,
                       Mmapper2Group &groupManager,
                       QWindow *parent = nullptr);
    ~MapCanvas() final;

public:
    NODISCARD static MapCanvas *getPrimary();

private:
    // ICanvasHost
    void requestCanvasUpdate() override { update(); }
    NODISCARD qreal hostDevicePixelRatio() const override { return devicePixelRatioF(); }
    NODISCARD QSize hostSize() const override
    {
        return QSize(QOpenGLWindow::width(), QOpenGLWindow::height());
    }
    void setHostCursor(const Qt::CursorShape shape) override { QOpenGLWindow::setCursor(shape); }

private:
    void cleanupOpenGL();

private slots:
    // Widget-facing reactions to MapCanvasCore's Qt-widgets-free failure
    // signals; see the comments on sig_glInitFailed/sig_glFatalError in
    // MapCanvasCore.h.
    void handleGlInitFailed(const QString &reason);
    void handleGlFatalError(const QString &message);

public:
    void shuttingDown();

public:
    void setZoom(float zoom) { m_core.setZoom(zoom); }
    NODISCARD float getRawZoom() const { return m_core.getRawZoom(); }
    NODISCARD float getTotalScaleFactor() const { return m_core.getTotalScaleFactor(); }

public:
    NODISCARD auto width() const { return QOpenGLWindow::width(); }
    NODISCARD auto height() const { return QOpenGLWindow::height(); }
    NODISCARD QRect rect() const { return QRect(0, 0, width(), height()); }

protected:
    void initializeGL() override;
    void paintGL() override { m_core.hostPaintGL(); }
    void resizeGL(int width, int height) override
    {
        m_core.hostResize(width, height, devicePixelRatioF());
    }
    void mousePressEvent(QMouseEvent *event) override { m_core.handleMousePress(event); }
    void mouseReleaseEvent(QMouseEvent *event) override { m_core.handleMouseRelease(event); }
    void mouseMoveEvent(QMouseEvent *event) override { m_core.handleMouseMove(event); }
    void wheelEvent(QWheelEvent *event) override { m_core.handleWheel(event); }
    void touchEvent(QTouchEvent *event) override { m_core.handleTouch(event); }
    bool event(QEvent *e) override
    {
        if (m_core.handleGenericEvent(e)) {
            return true;
        }
        return QOpenGLWindow::event(e);
    }

public:
    void userPressedEscape(bool pressed) { m_core.userPressedEscape(pressed); }
    void screenChanged() { m_core.screenChanged(); }
    void graphicsSettingsChanged() { m_core.graphicsSettingsChanged(); }

signals:
    void sig_onCenter(glm::vec2 worldCoord);
    void sig_mapMove(int dx, int dy);
    void sig_setScrollBars(Coordinate min, Coordinate max);
    void sig_continuousScroll(int, int);

    void sig_log(const QString &, const QString &);

    void sig_selectionChanged();
    void sig_newRoomSelection(const SigRoomSelection &);
    void sig_newConnectionSelection(ConnectionSelection *);
    void sig_newInfomarkSelection(InfomarkSelection *);

    void sig_setCurrentRoom(RoomId id, bool update);
    void sig_zoomChanged(float);

    void sig_showTooltip(const QString &text, const QPoint &pos);
    void sig_customContextMenuRequested(const QPoint &pos);
    void sig_dismissContextMenu();

public slots:
    void slot_onForcedPositionChange() { m_core.slot_onForcedPositionChange(); }
    void slot_createRoom() { m_core.slot_createRoom(); }

    void slot_setCanvasMouseMode(CanvasMouseModeEnum mode) { m_core.slot_setCanvasMouseMode(mode); }

    void slot_setScroll(const glm::vec2 worldPos) { m_core.slot_setScroll(worldPos); }
    void slot_setHorizontalScroll(float worldX) { m_core.slot_setHorizontalScroll(worldX); }
    void slot_setVerticalScroll(float worldY) { m_core.slot_setVerticalScroll(worldY); }

    void slot_zoomIn() { m_core.slot_zoomIn(); }
    void slot_zoomOut() { m_core.slot_zoomOut(); }
    void slot_zoomReset() { m_core.slot_zoomReset(); }

    void slot_layerUp() { m_core.slot_layerUp(); }
    void slot_layerDown() { m_core.slot_layerDown(); }
    void slot_layerReset() { m_core.slot_layerReset(); }

    void slot_setRoomSelection(const SigRoomSelection &sel) { m_core.slot_setRoomSelection(sel); }
    void slot_setConnectionSelection(const std::shared_ptr<ConnectionSelection> &sel)
    {
        m_core.slot_setConnectionSelection(sel);
    }
    void slot_setInfomarkSelection(const std::shared_ptr<InfomarkSelection> &sel)
    {
        m_core.slot_setInfomarkSelection(sel);
    }

    void slot_clearRoomSelection() { m_core.slot_clearRoomSelection(); }
    void slot_clearConnectionSelection() { m_core.slot_clearConnectionSelection(); }
    void slot_clearInfomarkSelection() { m_core.slot_clearInfomarkSelection(); }
    void slot_clearAllSelections() { m_core.slot_clearAllSelections(); }

    void slot_dataLoaded() { m_core.slot_dataLoaded(); }
    void slot_moveMarker(RoomId id) { m_core.slot_moveMarker(id); }

    void slot_rebuildMeshes() { m_core.slot_rebuildMeshes(); }
    void slot_mapChanged() { m_core.slot_mapChanged(); }
    void slot_requestUpdate() { m_core.slot_requestUpdate(); }
};
