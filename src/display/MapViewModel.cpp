// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "MapViewModel.h"

#include "mapcanvas.h"

#include <algorithm>

#include <QTimer>

static_assert(MapViewModel::SCROLL_SCALE == MapCanvasCore::SCROLL_SCALE,
              "MapViewModel::SCROLL_SCALE must be kept in sync with MapCanvasCore::SCROLL_SCALE");

MapViewModel::MapViewModel(QObject *const parent)
    : QObject(parent)
{
    m_scrollTimer = new QTimer(this);
    connect(m_scrollTimer, &QTimer::timeout, this, &MapViewModel::slot_scrollTimerTimeout);

    // Applies each continuous-scroll tick to the current scroll position,
    // mirroring MapWindow::slot_applyScrollStep() (mapwindow.cpp:238-250);
    // self-connected here (rather than left for a consumer to wire) since
    // slot_continuousScroll() already negates vStep for world space (see
    // below), so no further axis flip is needed before applying it.
    connect(this, &MapViewModel::sig_continuousScrollStep, this, &MapViewModel::applyScrollDelta);
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

void MapViewModel::applyScrollDelta(const int dh, const int dv)
{
    // Ported from MapWindow::slot_mapMove()/slot_applyScrollStep()
    // (mapwindow.cpp:216-250): both add a scroll-bar-space delta to the
    // current QScrollBar values (which self-clamp to their [0, max] range
    // via QScrollBar::setValue()) and re-derive/re-emit the resulting world
    // position.
    m_horizontalScrollValue = std::clamp(m_horizontalScrollValue + dh, 0, m_horizontalScrollMax);
    m_verticalScrollValue = std::clamp(m_verticalScrollValue + dv, 0, m_verticalScrollMax);
    emit sig_valueChanged();
    const auto worldPos = scrollToWorld(glm::ivec2{m_horizontalScrollValue, m_verticalScrollValue});
    emit sig_scrollToWorld(worldPos);
}

void MapViewModel::slot_centerOnWorldPos(const glm::vec2 worldPos)
{
    // Ported from MapWindow::slot_centerOnWorldPos() (mapwindow.cpp:257-269).
    const auto scrollPos = worldToScroll(worldPos);
    m_horizontalScrollValue = std::clamp(scrollPos.x, 0, m_horizontalScrollMax);
    m_verticalScrollValue = std::clamp(scrollPos.y, 0, m_verticalScrollMax);
    emit sig_valueChanged();
    // Re-emits the ORIGINAL worldPos, not the clamped-then-reconverted
    // value, matching MapWindow's `emit sig_setScroll(worldPos)`.
    emit sig_scrollToWorld(worldPos);
}

void MapViewModel::slot_mapMove(const int dx, const int input_dy)
{
    // Ported from MapWindow::slot_mapMove() (mapwindow.cpp:216-231): Y is
    // negated because the delta arrives in world space.
    const int dy = -input_dy;
    applyScrollDelta(dx, dy);
}

void MapViewModel::setHorizontalScrollValue(const int value)
{
    // Ported from MapWindow's `connect(m_horizontalScrollBar,
    // &QScrollBar::valueChanged, ...)` lambda (mapwindow.cpp:153-159): only
    // this axis's world coordinate is recomputed and re-emitted, unlike
    // applyScrollDelta()/slot_centerOnWorldPos() which touch both axes.
    const int clamped = std::clamp(value, 0, m_horizontalScrollMax);
    if (clamped == m_horizontalScrollValue) {
        return;
    }
    m_horizontalScrollValue = clamped;
    emit sig_valueChanged();
    const auto worldX = scrollToWorld(glm::ivec2{m_horizontalScrollValue, m_verticalScrollValue}).x;
    emit sig_scrollWorldXChanged(worldX);
}

void MapViewModel::setVerticalScrollValue(const int value)
{
    // Ported from MapWindow's `connect(m_verticalScrollBar,
    // &QScrollBar::valueChanged, ...)` lambda (mapwindow.cpp:161-167).
    const int clamped = std::clamp(value, 0, m_verticalScrollMax);
    if (clamped == m_verticalScrollValue) {
        return;
    }
    m_verticalScrollValue = clamped;
    emit sig_valueChanged();
    const auto worldY = scrollToWorld(glm::ivec2{m_horizontalScrollValue, m_verticalScrollValue}).y;
    emit sig_scrollWorldYChanged(worldY);
}
