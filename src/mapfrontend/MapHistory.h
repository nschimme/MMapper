#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2023 The MMapper Authors

#include "../map/Map.h" // Assuming Map class is in this path
#include <deque>
#include <cstddef> // For size_t

class MapHistory
{
public:
    // Constructor
    MapHistory(bool capped = false, size_t max_size = 0);

    // Modifiers
    void push(Map map);
    Map pop(); // Returns and removes the top element
    void clear();

    // Capacity
    bool isEmpty() const;
    size_t size() const;

    // Element access
    const Map& top() const; // Returns the top element without removing it

    // Configuration
    void setCapped(bool capped);
    void setMaxSize(size_t max_size);

private:
    std::deque<Map> m_history;
    bool m_capped;
    size_t m_max_size;
};
