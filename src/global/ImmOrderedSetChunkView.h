#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2024 The MMapper Authors

#include "macros.h" // For NODISCARD
#include <vector>
#include <numeric>   // For std::distance, std::advance (though might not be used directly in iter ops)
#include <iterator>  // For std::random_access_iterator_tag, std::ptrdiff_t
#include <algorithm> // For std::upper_bound
#include <stdexcept> // For std::out_of_range

#include <immer/flex_vector.hpp> // Needs to know about flex_vector for constructor
#include <immer/algorithm.hpp>   // For immer::for_each_chunk

// Forward declaration for friendship or if ImmOrderedSet includes this
// template<typename U> class ImmOrderedSet;

namespace detail {

template<typename T>
class ChunkedView {
public:
    struct ChunkInfo {
        const T* begin;
        const T* end;
        NODISCARD size_t size() const { return static_cast<size_t>(std::distance(begin, end)); }
    };

private:
    friend class Iterator; // Iterator needs access to these members

    std::vector<ChunkInfo> m_collected_chunks;
    // m_cumulative_sizes[i] = total elements strictly BEFORE chunk i (i.e., global index of first element of chunk i)
    // m_cumulative_sizes[0] = 0.
    // Last element (m_cumulative_sizes[m_collected_chunks.size()]) is m_total_size.
    std::vector<size_t> m_cumulative_sizes;
    size_t m_total_size;

public:
    // Constructor
    explicit ChunkedView(const immer::flex_vector<T>& vector_container) : m_total_size(0) {
        immer::for_each_chunk(vector_container, [&](const T* chunk_begin, const T* chunk_end) {
            if (chunk_begin != chunk_end) {
                m_collected_chunks.push_back({chunk_begin, chunk_end});
            }
        });

        m_cumulative_sizes.push_back(0); // Base for the first chunk
        for (const auto& chunk_info : m_collected_chunks) {
            m_total_size += chunk_info.size();
            m_cumulative_sizes.push_back(m_total_size);
        }
    }

    // Default constructor for an empty view
    ChunkedView() : m_total_size(0) {
        m_cumulative_sizes.push_back(0);
    }


public: // Public Iterator definition
    class Iterator {
    public:
        // Iterator traits
        using iterator_category = std::random_access_iterator_tag;
        using value_type        = T;
        using difference_type   = std::ptrdiff_t;
        using pointer           = const T*;
        using reference         = const T&;

    private:
        const ChunkedView* m_view_ptr = nullptr;
        size_t m_current_chunk_idx = 0;     // Index of current chunk in m_view_ptr->m_collected_chunks
        const T* m_current_element_ptr = nullptr; // Pointer to the current element. nullptr if at end or view is empty.

        friend class ChunkedView<T>;

        // Private constructor for use by ChunkedView's begin/end
        Iterator(const ChunkedView* view, size_t chunk_idx, const T* element_ptr)
            : m_view_ptr(view), m_current_chunk_idx(chunk_idx), m_current_element_ptr(element_ptr) {}

    public:
        // Default constructor: creates a singular iterator (often points to "end" or is invalid)
        Iterator() = default;

        reference operator*() const {
            // TODO: Add assertion: m_current_element_ptr != nullptr
            return *m_current_element_ptr;
        }
        pointer operator->() const { return m_current_element_ptr; }

        Iterator& operator++() {
            if (m_current_element_ptr == nullptr) { // Already at end
                return *this;
            }

            ++m_current_element_ptr; // Move to next element in current physical chunk

            // Check if we moved past the end of the current physical chunk
            if (m_current_element_ptr == m_view_ptr->m_collected_chunks[m_current_chunk_idx].end) {
                ++m_current_chunk_idx; // Move to the next chunk_info
                if (m_current_chunk_idx < m_view_ptr->m_collected_chunks.size()) {
                    // If there is a next chunk, point to its beginning
                    m_current_element_ptr = m_view_ptr->m_collected_chunks[m_current_chunk_idx].begin;
                } else {
                    // No more chunks, so this is the end iterator
                    m_current_element_ptr = nullptr;
                    // m_current_chunk_idx is now m_view_ptr->m_collected_chunks.size()
                }
            }
            return *this;
        }

        Iterator operator++(int) { Iterator temp = *this; ++(*this); return temp; }

        Iterator& operator--() {
            if (m_current_chunk_idx == 0 &&
                (m_current_element_ptr == nullptr || (m_view_ptr->empty() ? true : m_current_element_ptr == m_view_ptr->m_collected_chunks[0].begin))) {
                // At the beginning (or was end of empty list), cannot decrement further
                // Or handle as undefined behavior for decrementing begin()
                return *this;
            }

            if (m_current_element_ptr == nullptr) { // Currently at end()
                // Move to the last element of the last chunk
                --m_current_chunk_idx; // Should be m_view_ptr->m_collected_chunks.size() - 1
                m_current_element_ptr = m_view_ptr->m_collected_chunks[m_current_chunk_idx].end - 1;
            } else if (m_current_element_ptr == m_view_ptr->m_collected_chunks[m_current_chunk_idx].begin) {
                 // At the beginning of a chunk (but not the first chunk overall)
                --m_current_chunk_idx;
                m_current_element_ptr = m_view_ptr->m_collected_chunks[m_current_chunk_idx].end - 1;
            } else {
                --m_current_element_ptr;
            }
            return *this;
        }

        Iterator operator--(int) { Iterator temp = *this; --(*this); return temp; }

        size_t get_global_index() const {
            if (m_view_ptr == nullptr || m_current_element_ptr == nullptr) {
                // This is the end iterator or an uninitialized/empty view's iterator
                return m_view_ptr ? m_view_ptr->m_total_size : 0;
            }
            // Assertion: m_current_chunk_idx < m_view_ptr->m_collected_chunks.size()
            size_t base_idx = m_view_ptr->m_cumulative_sizes[m_current_chunk_idx];
            size_t offset_in_chunk = static_cast<size_t>(m_current_element_ptr - m_view_ptr->m_collected_chunks[m_current_chunk_idx].begin);
            return base_idx + offset_in_chunk;
        }

        Iterator& operator+=(difference_type n) {
            if (n == 0) return *this;

            size_t current_global = get_global_index();
            difference_type target_global_signed = static_cast<difference_type>(current_global) + n;
            size_t target_global;

            if (target_global_signed < 0) {
                // TODO: Handle advancing before beginning (throw, assert, or specific state)
                // For now, let's assume it results in an invalid state or clamps.
                // This state is not well-defined by simply moving to chunk 0, element 0.
                // Let's make it point to "before begin" by setting to end state and then trying to decrement.
                // This part is tricky for RA iterators. Simplest is to disallow or assert valid range.
                // Or define it as moving to begin if target < 0.
                target_global = 0; // Clamp to beginning for simplicity in this draft
            } else {
                target_global = static_cast<size_t>(target_global_signed);
            }

            if (target_global >= m_view_ptr->m_total_size) {
                m_current_chunk_idx = m_view_ptr->m_collected_chunks.size();
                m_current_element_ptr = nullptr; // End state
            } else {
                // Find which chunk this global_idx falls into using cumulative_sizes
                // m_cumulative_sizes[k] <= global_idx < m_cumulative_sizes[k+1]
                // upper_bound returns iterator to first element > global_idx
                auto bound_it = std::upper_bound(m_view_ptr->m_cumulative_sizes.begin() + 1,
                                                 m_view_ptr->m_cumulative_sizes.end(),
                                                 target_global);
                m_current_chunk_idx = static_cast<size_t>(std::distance(m_view_ptr->m_cumulative_sizes.begin(), bound_it) - 1);

                size_t index_within_chunk = target_global - m_view_ptr->m_cumulative_sizes[m_current_chunk_idx];
                m_current_element_ptr = m_view_ptr->m_collected_chunks[m_current_chunk_idx].begin + index_within_chunk;
            }
            return *this;
        }

        Iterator& operator-=(difference_type n) {
            return operator+=(-n);
        }

        NODISCARD Iterator operator+(difference_type n) const {
            Iterator temp = *this;
            temp += n;
            return temp;
        }

        NODISCARD Iterator operator-(difference_type n) const {
            Iterator temp = *this;
            temp -= n;
            return temp;
        }

        difference_type operator-(const Iterator& other) const {
            return static_cast<difference_type>(get_global_index()) - static_cast<difference_type>(other.get_global_index());
        }

        reference operator[](difference_type n) const {
            return *(*this + n);
        }

        // Comparison Operators
        bool operator==(const Iterator& other) const {
            // Primary check on element pointer. If both are null, they must be same type of end.
            if (m_current_element_ptr == other.m_current_element_ptr) {
                return m_current_element_ptr != nullptr || m_current_chunk_idx == other.m_current_chunk_idx;
            }
            return false;
        }
        bool operator!=(const Iterator& other) const { return !(*this == other); }
        bool operator<(const Iterator& other) const { return get_global_index() < other.get_global_index(); }
        bool operator>(const Iterator& other) const { return get_global_index() > other.get_global_index(); }
        bool operator<=(const Iterator& other) const { return get_global_index() <= other.get_global_index(); }
        bool operator>=(const Iterator& other) const { return get_global_index() >= other.get_global_index(); }
    }; // End of Iterator class

public: // Back to ChunkedView methods
    NODISCARD Iterator begin() const {
        if (empty()) {
            return Iterator(this, m_collected_chunks.size(), nullptr);
        }
        return Iterator(this, 0, m_collected_chunks[0].begin);
    }

    NODISCARD Iterator end() const {
        return Iterator(this, m_collected_chunks.size(), nullptr);
    }

    NODISCARD size_t size() const {
        return m_total_size;
    }

    NODISCARD bool empty() const {
        return m_total_size == 0;
    }
}; // End of ChunkedView class

} // namespace detail
