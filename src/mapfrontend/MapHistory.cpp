// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2023 The MMapper Authors

#include "MapHistory.h"
#include <stdexcept> // For std::out_of_range

// Constructor
MapHistory::MapHistory(bool capped, size_t max_size)
    : m_capped(capped), m_max_size(max_size) {}

// Modifiers
void MapHistory::push(Map map)
{
    m_history.push_back(std::move(map));
    if (m_capped && m_history.size() > m_max_size)
    {
        if (!m_history.empty()) // Should always be true here, but good practice
        {
            m_history.pop_front();
        }
    }
}

Map MapHistory::pop()
{
    if (m_history.empty())
    {
        // Or throw an exception, depending on desired error handling
        // For now, let's assume the caller checks isEmpty()
        // However, throwing is safer to prevent undefined behavior with an empty deque.
        throw std::out_of_range("Cannot pop from an empty MapHistory");
    }
    Map top_map = std::move(m_history.back());
    m_history.pop_back();
    return top_map;
}

void MapHistory::clear()
{
    m_history.clear();
}

// Capacity
bool MapHistory::isEmpty() const
{
    return m_history.empty();
}

size_t MapHistory::size() const
{
    return m_history.size();
}

// Element access
const Map& MapHistory::top() const
{
    if (m_history.empty())
    {
        // Similar to pop(), throwing is safer.
        throw std::out_of_range("Cannot access top element of an empty MapHistory");
    }
    return m_history.back();
}

// Configuration
void MapHistory::setCapped(bool capped)
{
    m_capped = capped;
}

void MapHistory::setMaxSize(size_t max_size)
{
    m_max_size = max_size;
    // Optionally, enforce max_size immediately if current size exceeds new max_size
    if (m_capped) {
        while (m_history.size() > m_max_size && !m_history.empty()) {
            m_history.pop_front();
        }
    }
}
