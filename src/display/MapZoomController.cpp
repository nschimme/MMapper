// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "MapZoomController.h"

#include "MapCanvasCore.h"

#include <algorithm>
#include <cmath>

int MapZoomController::calcPos(const float zoom) noexcept
{
    static const float INV_DIVISOR = 1.f / std::log2(ScaleFactor::ZOOM_STEP);
    return static_cast<int>(std::lround(SCALE * std::log2(zoom) * INV_DIVISOR));
}

MapZoomController::MapZoomController(MapCanvasCore &canvas, QObject *const parent)
    : QObject(parent)
    , m_canvas{canvas}
{
    setFromActual();
    connect(&canvas, &MapCanvasCore::sig_zoomChanged, this, [this](float) { setFromActual(); });
}

void MapZoomController::setPosition(const int position)
{
    const int clamped = clamp(position);
    if (clamped == m_position) {
        return;
    }
    m_position = clamped;

    const float desiredZoomSteps = static_cast<float>(clamped) * INV_SCALE;
    m_canvas.setZoom(std::pow(ScaleFactor::ZOOM_STEP, desiredZoomSteps));

    emit sig_positionChanged(m_position);
}

void MapZoomController::setFromActual()
{
    const float actualZoom = m_canvas.getRawZoom();
    const int rounded = clamp(calcPos(actualZoom));
    if (rounded == m_position) {
        return;
    }
    m_position = rounded;
    emit sig_positionChanged(m_position);
}
