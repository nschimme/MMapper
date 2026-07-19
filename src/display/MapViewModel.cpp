// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "MapViewModel.h"

#include "mapcanvas.h"

#include <QTimer>

static_assert(MapViewModel::SCROLL_SCALE == MapCanvasCore::SCROLL_SCALE,
              "MapViewModel::SCROLL_SCALE must be kept in sync with MapCanvasCore::SCROLL_SCALE");

MapViewModel::MapViewModel(QObject *const parent)
    : QObject(parent)
{
    m_scrollTimer = new QTimer(this);
    connect(m_scrollTimer, &QTimer::timeout, this, &MapViewModel::slot_scrollTimerTimeout);
}

MapViewModel::~MapViewModel() = default;

void MapViewModel::setZoom(const float zoom)
{
    if (utils::equals(zoom, m_zoom)) {
        return;
    }
    m_zoom = zoom;
    emit sig_zoomChanged(zoom);
}

void MapViewModel::slot_setScrollBars(const Coordinate min, const Coordinate max)
{
    m_knownMapSize.min = min.to_ivec3();
    m_knownMapSize.max = max.to_ivec3();

    const auto dims = m_knownMapSize.size() * SCROLL_SCALE;
    m_horizontalScrollMax = dims.x;
    m_verticalScrollMax = dims.y;
    emit sig_rangeChanged();
}

// REVISIT: This looks more like "delayed jump" than "continuous scroll."
// (comment ported verbatim from MapWindow::slot_continuousScroll())
void MapViewModel::slot_continuousScroll(const int hStep, const int input_vStep)
{
    const auto fitsInInt8 = [](int n) -> bool {
        // alternate: test against std::numeric_limits<int8_t>::min and max.
        return static_cast<int>(static_cast<int8_t>(n)) == n;
    };

    // code originally used int8_t
    assert(fitsInInt8(hStep));
    assert(fitsInInt8(input_vStep));

    // Y is negated because delta is in world space
    const int vStep = -input_vStep;

    m_horizontalScrollStep = hStep;
    m_verticalScrollStep = vStep;

    auto &scrollTimer = *m_scrollTimer;
    // stop
    if (hStep == 0 && vStep == 0) {
        if (scrollTimer.isActive()) {
            scrollTimer.stop();
        }
    } else {
        // start
        if (!scrollTimer.isActive()) {
            scrollTimer.start(100);
        }
    }
}

void MapViewModel::slot_scrollTimerTimeout()
{
    emit sig_continuousScrollStep(m_horizontalScrollStep, m_verticalScrollStep);
}

glm::vec2 MapViewModel::KnownMapSize::scrollToWorld(const glm::ivec2 scrollPos) const
{
    auto worldPos = glm::vec2{scrollPos} / static_cast<float>(MapViewModel::SCROLL_SCALE);
    worldPos.y = static_cast<float>(size().y) - worldPos.y; // negate Y
    worldPos += glm::vec2{min};
    return worldPos;
}

glm::ivec2 MapViewModel::KnownMapSize::worldToScroll(const glm::vec2 worldPos_in) const
{
    auto worldPos = worldPos_in;
    worldPos -= glm::vec2{min};
    worldPos.y = static_cast<float>(size().y) - worldPos.y; // negate Y
    const glm::ivec2 scrollPos{worldPos * static_cast<float>(MapViewModel::SCROLL_SCALE)};
    return scrollPos;
}
