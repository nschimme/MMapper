// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019-2024 The MMapper Authors

#include "MapZoomSlider.h"
#include "../display/mapwindow.h"
#include <cmath>

int MapZoomSlider::calcPos(const float z) noexcept { return static_cast<int>(std::round(std::log2(z) * 100.0f)); }
MapZoomSlider::MapZoomSlider(MapWindow &m, QWidget *p) : QSlider(Qt::Horizontal, p), m_map(m) {
    setRange(calcPos(ScaleFactor::MIN_VALUE), calcPos(ScaleFactor::MAX_VALUE));
    connect(this, &QSlider::valueChanged, this, [this](int v) { m_viewModel.setSliderValue(v); requestChange(); });
    connect(&m_viewModel, &MapZoomSliderViewModel::sliderValueChanged, this, [this]() { if (value() != m_viewModel.sliderValue()) setValue(m_viewModel.sliderValue()); });
}
MapZoomSlider::~MapZoomSlider() = default;
void MapZoomSlider::requestChange() { m_map.setZoom(m_viewModel.zoomValue()); }
void MapZoomSlider::setFromActual() { m_viewModel.setZoomValue(m_map.getRawZoom()); }
