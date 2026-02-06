// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "mapwindow.h"
#include "../display/Filenames.h"
#include "../global/SignalBlocker.h"
#include "../configuration/configuration.h"

MapWindow::MapWindow(MapData &mapData, PrespammedPath &pp, Mmapper2Group &gm, QWidget *parent)
    : QWidget(parent)
{
    m_viewModel = std::make_unique<MapWindowViewModel>(this);
    m_gridLayout = std::make_unique<QGridLayout>(this);
    m_gridLayout->setSpacing(0);
    m_gridLayout->setContentsMargins(0, 0, 0, 0);

    m_verticalScrollBar = std::make_unique<QScrollBar>(this);
    m_verticalScrollBar->setOrientation(Qt::Vertical);
    m_gridLayout->addWidget(m_verticalScrollBar.get(), 0, 1);

    m_horizontalScrollBar = std::make_unique<QScrollBar>(this);
    m_horizontalScrollBar->setOrientation(Qt::Horizontal);
    m_gridLayout->addWidget(m_horizontalScrollBar.get(), 1, 0);

    m_canvas = std::make_unique<MapCanvas>(mapData, pp, gm, this);
    m_gridLayout->addWidget(m_canvas.get(), 0, 0);

    connect(m_horizontalScrollBar.get(), &QScrollBar::valueChanged, [this](int x) {
        emit sig_setScroll(m_viewModel->scrollToWorld(glm::ivec2(x, m_verticalScrollBar->value())));
    });
    connect(m_verticalScrollBar.get(), &QScrollBar::valueChanged, [this](int y) {
        emit sig_setScroll(m_viewModel->scrollToWorld(glm::ivec2(m_horizontalScrollBar->value(), y)));
    });

    connect(m_canvas.get(), &MapCanvas::sig_onCenter, this, &MapWindow::slot_centerOnWorldPos);
    connect(m_canvas.get(), &MapCanvas::sig_setScrollBars, this, &MapWindow::slot_setScrollBars);
    connect(m_canvas.get(), &MapCanvas::sig_mapMove, this, &MapWindow::slot_mapMove);
    connect(m_canvas.get(), &MapCanvas::sig_zoomChanged, this, &MapWindow::slot_zoomChanged);

    connect(this, &MapWindow::sig_setScroll, m_canvas.get(), &MapCanvas::slot_setScroll);
}

MapWindow::~MapWindow() = default;

void MapWindow::hideSplashImage() { if (m_splashLabel) m_splashLabel->hide(); }

void MapWindow::slot_setScrollBars(const Coordinate &min, const Coordinate &max) {
    m_viewModel->setMapRange(min.to_ivec3(), max.to_ivec3());
    updateScrollBars();
}

void MapWindow::updateScrollBars() {
    // simplified
    m_horizontalScrollBar->setVisible(getConfig().general.showScrollBars);
    m_verticalScrollBar->setVisible(getConfig().general.showScrollBars);
}

void MapWindow::slot_centerOnWorldPos(const glm::vec2 &worldPos) {
    glm::ivec2 scroll = m_viewModel->worldToScroll(worldPos);
    SignalBlocker sb1(*m_horizontalScrollBar);
    SignalBlocker sb2(*m_verticalScrollBar);
    m_horizontalScrollBar->setValue(scroll.x);
    m_verticalScrollBar->setValue(scroll.y);
    emit sig_setScroll(worldPos);
}

void MapWindow::slot_mapMove(int dx, int dy) {
    slot_centerOnWorldPos(m_viewModel->scrollToWorld(glm::ivec2(m_horizontalScrollBar->value() + dx, m_verticalScrollBar->value() - dy)));
}
