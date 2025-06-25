#pragma once

#include "../map/Map.h"
#include <deque>
#include <cstddef>   // For size_t
#include <limits>    // For std::numeric_limits
#include <stdexcept> // For std::runtime_error

class MapHistory {
private:
    std::deque<Map> m_history;
    size_t m_max_size; // If std::numeric_limits<size_t>::max(), effectively unlimited for front-popping due to cap.

public:
    // Constructor:
    // max_size_val: The maximum number of items to store.
    // Use std::numeric_limits<size_t>::max() for effectively unlimited (no capping by removing oldest from front).
    // A max_size_val of 0 means the history will not store any items (push will be a no-op).
    explicit MapHistory(size_t max_size_val) : m_max_size(max_size_val) {}

    void push(const Map& map) {
        // If max_size is 0, this history is configured to hold nothing.
        if (m_max_size == 0) {
            return;
        }

        // If the history is already at its configured maximum size (and not "unlimited" for this purpose),
        // remove the oldest element (from the front) to make space.
        // This check ensures we don't pop if m_max_size is SIZE_MAX.
        if (m_max_size != std::numeric_limits<size_t>::max() && m_history.size() >= m_max_size) {
            if (!m_history.empty()) { // This check is mostly for safety, should be true if size >= max_size > 0
                m_history.pop_front();
            }
        }
        m_history.push_back(map);
    }

    Map pop() {
        if (isEmpty()) {
            throw std::runtime_error("Cannot pop from an empty MapHistory");
        }
        Map top_map = m_history.back();
        m_history.pop_back();
        return top_map; // RVO or move should occur if Map is movable
    }

    const Map& top() const {
        if (isEmpty()) {
            throw std::runtime_error("Cannot get top from an empty MapHistory");
        }
        return m_history.back();
    }

    bool isEmpty() const {
        return m_history.empty();
    }

    size_t size() const {
        return m_history.size();
    }

    void clear() {
        m_history.clear();
    }
};
