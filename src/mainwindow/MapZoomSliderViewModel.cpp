// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#include "MapZoomSliderViewModel.h"
#include <cmath>
#include <algorithm>

static constexpr float SCALE = 100.f;

MapZoomSliderViewModel::MapZoomSliderViewModel(QObject *p) : QObject(p) {}
void MapZoomSliderViewModel::setSliderValue(int v) {
    if (m_sliderValue != v) {
        m_sliderValue = v;
        m_zoomValue = std::pow(2.0f, v / SCALE);
        emit sliderValueChanged();
        emit zoomValueChanged();
    }
}
void MapZoomSliderViewModel::setZoomValue(float z) {
    if (m_zoomValue != z) {
        m_zoomValue = z;
        m_sliderValue = static_cast<int>(std::round(std::log2(z) * SCALE));
        emit sliderValueChanged();
        emit zoomValueChanged();
    }
}
