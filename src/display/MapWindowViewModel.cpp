// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#include "MapWindowViewModel.h"
#include "mapcanvas.h"
#include <algorithm>

MapWindowViewModel::MapWindowViewModel(QObject *parent) : QObject(parent) {}

void MapWindowViewModel::setScrollPos(const glm::vec2 &pos) {
    if (m_scrollPos != pos) {
        m_scrollPos = pos;
        emit scrollPosChanged();
    }
}

void MapWindowViewModel::setMapRange(const glm::ivec3 &min, const glm::ivec3 &max) {
    m_min = min;
    m_max = max;
}

glm::vec2 MapWindowViewModel::scrollToWorld(const glm::ivec2 &scrollPos) const {
    auto worldPos = glm::vec2{scrollPos} / static_cast<float>(MapCanvas::SCROLL_SCALE);
    worldPos.y = static_cast<float>(m_max.y - m_min.y) - worldPos.y;
    worldPos += glm::vec2{m_min.x, m_min.y};
    return worldPos;
}

glm::ivec2 MapWindowViewModel::worldToScroll(const glm::vec2 &worldPos_in) const {
    auto worldPos = worldPos_in;
    worldPos -= glm::vec2{m_min.x, m_min.y};
    worldPos.y = static_cast<float>(m_max.y - m_min.y) - worldPos.y;
    return glm::ivec2{worldPos * static_cast<float>(MapCanvas::SCROLL_SCALE)};
}
