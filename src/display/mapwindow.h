#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Ulf Hermann <ulfonk_mennhar@gmx.de> (Alve)
// Author: Marek Krejza <krejza@gmail.com> (Caligor)
// Author: Nils Schimmelmann <nschimme@gmail.com> (Jahara)

#include "../map/coordinate.h"
#include "MapViewModel.h"
#include "mapcanvas.h"

#include <QLabel>
#include <QPixmap>
#include <QPoint>
#include <QPointer>
#include <QSize>
#include <QString>
#include <QWidget>
#include <QtCore>
#include <QtGlobal>

class AudioHintWidget;
class GameObserver;
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

class NODISCARD_QOBJECT MapWindow final : public QWidget
{
    Q_OBJECT

protected:
    QPointer<QGridLayout> m_gridLayout;
    QPointer<QScrollBar> m_horizontalScrollBar;
    QPointer<QScrollBar> m_verticalScrollBar;
    QPointer<MapCanvas> m_canvas;
    QPointer<QWidget> m_canvasContainer;
    QPointer<QWidget> m_splashWidget;
    QPointer<AudioHintWidget> m_audioHint;

private:
    // Scroll math (world<->scroll-unit conversion) and the continuous-scroll
    // timer live in MapViewModel now (see MapViewModel.h/.cpp), shared with
    // a future QML map view. MapWindow still owns the QScrollBar widgets
    // themselves and the glue that reads/writes their values.
    MapViewModel *m_viewModel = nullptr;

public:
    explicit MapWindow(MapData &mapData,
                       GameObserver &observer,
                       PrespammedPath &pp,
                       Mmapper2Group &gm,
                       QWidget *parent);
    ~MapWindow() final;

public:
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

    NODISCARD MapCanvas *getCanvas() const;

public:
    void updateScrollBars();
    void setZoom(float zoom);
    NODISCARD float getZoom() const;
    void hideSplashImage();

private:
    void centerOnScrollPos(glm::ivec2 scrollPos);

signals:
    void sig_setScroll(glm::vec2 worldPos);
    void sig_zoomChanged(float zoom);

public slots:
    void slot_setScrollBars(Coordinate, Coordinate);
    void slot_centerOnWorldPos(glm::vec2 worldPos);
    void slot_mapMove(int dx, int dy);
    void slot_continuousScroll(int dx, int dy);
    void slot_graphicsSettingsChanged();
    void slot_zoomChanged(const float zoom) { emit sig_zoomChanged(zoom); }
    void slot_showTooltip(const QString &text, const QPoint &pos);

    void setCanvasEnabled(bool enabled);

private slots:
    // Connected to MapViewModel::sig_continuousScrollStep(); applies one
    // 100ms continuous-scroll tick to the scrollbars (formerly
    // MapWindow::slot_scrollTimerTimeout(), before the timer moved into
    // MapViewModel).
    void slot_applyScrollStep(int hStep, int vStep);
};
