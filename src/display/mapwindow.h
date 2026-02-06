#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "../map/coordinate.h"
#include "mapcanvas.h"
#include "MapWindowViewModel.h"

#include <memory>
#include <QLabel>
#include <QScrollBar>
#include <QGridLayout>

class MapData;
class Mmapper2Group;
class PrespammedPath;

class NODISCARD_QOBJECT MapWindow final : public QWidget
{
    Q_OBJECT
private:
    std::unique_ptr<MapWindowViewModel> m_viewModel;
    std::unique_ptr<QGridLayout> m_gridLayout;
    std::unique_ptr<QScrollBar> m_horizontalScrollBar;
    std::unique_ptr<QScrollBar> m_verticalScrollBar;
    std::unique_ptr<MapCanvas> m_canvas;
    std::unique_ptr<QLabel> m_splashLabel;

public:
    explicit MapWindow(MapData &mapData, PrespammedPath &pp, Mmapper2Group &gm, QWidget *parent);
    ~MapWindow() final;

    MapCanvas *getCanvas() const { return m_canvas.get(); }
    void updateScrollBars();
    void hideSplashImage();

signals:
    void sig_setScroll(const glm::vec2 &worldPos);
    void sig_zoomChanged(float zoom);

public slots:
    void slot_setScrollBars(const Coordinate &min, const Coordinate &max);
    void slot_centerOnWorldPos(const glm::vec2 &worldPos);
    void slot_mapMove(int dx, int dy);
    void slot_zoomChanged(float zoom) { emit sig_zoomChanged(zoom); }
};
