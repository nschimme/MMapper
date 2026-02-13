#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Ulf Hermann <ulfonk_mennhar@gmx.de> (Alve)
// Author: Marek Krejza <krejza@gmail.com> (Caligor)
// Author: Nils Schimmelmann <nschimme@gmail.com> (Jahara)

#include "../map/coordinate.h"
#include "MapCanvasData.h"
#include "mapcanvas.h"

#include <memory>

#include <QGestureEvent>
#include <QLabel>
#include <QPixmap>
#include <QPoint>
#include <QSize>
#include <QString>
#include <QWidget>
#include <QtCore>
#include <QtGlobal>

class MapCanvas;
class MapData;
class Mmapper2Group;
class PrespammedPath;
class QGridLayout;
class QKeyEvent;
class QMouseEvent;
class QObject;
class QResizeEvent;
class QScrollBar;
class QTimer;

class NODISCARD_QOBJECT MapWindow final : public QWidget, public MapCanvasInputState
{
    Q_OBJECT

protected:
    std::unique_ptr<QTimer> scrollTimer;
    int m_verticalScrollStep = 0;
    int m_horizontalScrollStep = 0;

    std::unique_ptr<QGridLayout> m_gridLayout;
    std::unique_ptr<QScrollBar> m_horizontalScrollBar;
    std::unique_ptr<QScrollBar> m_verticalScrollBar;
    MapCanvas *m_canvas = nullptr;
    QWidget *m_canvasContainer = nullptr;
    std::unique_ptr<QLabel> m_splashLabel;

    struct AltDragState
    {
        QPoint lastPos;
        QCursor originalCursor;
    };
    std::optional<AltDragState> m_altDragState;

    MapData &m_data;

private:
    struct NODISCARD KnownMapSize final
    {
        glm::ivec3 min{0};
        glm::ivec3 max{0};

        NODISCARD glm::ivec2 size() const { return glm::ivec2{max - min}; }

        NODISCARD glm::vec2 scrollToWorld(const glm::ivec2 &scrollPos) const;
        NODISCARD glm::ivec2 worldToScroll(const glm::vec2 &worldPos) const;

    } m_knownMapSize;

public:
    explicit MapWindow(MapData &mapData, PrespammedPath &pp, Mmapper2Group &gm, QWidget *parent);
    ~MapWindow() final;

public:
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

    bool eventFilter(QObject *watched, QEvent *event) override;

    NODISCARD MapCanvas *getMapCanvas() const;
    NODISCARD QWidget *getCanvas() const { return m_canvasContainer; }

private:
    void handleMousePress(QMouseEvent *event);
    void handleMouseRelease(QMouseEvent *event);
    void handleMouseMove(QMouseEvent *event);
    void handleWheel(QWheelEvent *event);
    void handleGesture(QGestureEvent *event);

    NODISCARD std::shared_ptr<InfomarkSelection> getInfomarkSelection(const MouseSel &sel);

public:
    void setMouseTracking(bool enable);
    void updateScrollBars();
    void setZoom(float zoom);
    NODISCARD float getZoom() const;
    void hideSplashImage();

private:
    void centerOnScrollPos(const glm::ivec2 &scrollPos);

public:
    void zoomChanged();

signals:
    void sig_onCenter(const glm::vec2 &worldCoord);
    void sig_mapMove(int dx, int dy);
    void sig_setScrollBars(const Coordinate &min, const Coordinate &max);
    void sig_continuousScroll(int, int);

    void sig_log(const QString &, const QString &);

    void sig_selectionChanged();
    void sig_newRoomSelection(const SigRoomSelection &);
    void sig_newConnectionSelection(ConnectionSelection *);
    void sig_newInfomarkSelection(InfomarkSelection *);

    void sig_setCurrentRoom(RoomId id, bool update);
    void sig_zoomChanged(float);

    void sig_setScroll(const glm::vec2 &worldPos);
    void sig_customContextMenuRequested(const QPoint &pos);

public slots:
    void slot_setScrollBars(const Coordinate &, const Coordinate &);
    void slot_centerOnWorldPos(const glm::vec2 &worldPos);
    void slot_mapMove(int dx, int dy);
    void slot_continuousScroll(int dx, int dy);
    void slot_scrollTimerTimeout();
    void slot_graphicsSettingsChanged();
    void slot_zoomChanged(const float zoom) { emit sig_zoomChanged(zoom); }

    void slot_setCanvasMouseMode(CanvasMouseModeEnum mode);

    void slot_setRoomSelection(const SigRoomSelection &);
    void slot_setConnectionSelection(const std::shared_ptr<ConnectionSelection> &);
    void slot_setInfomarkSelection(const std::shared_ptr<InfomarkSelection> &);

    void slot_clearRoomSelection();
    void slot_clearConnectionSelection();
    void slot_clearInfomarkSelection();
    void slot_clearAllSelections();

    void slot_createRoom();
};
