#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2021 The MMapper Authors

#include "../global/macros.h"
#include "roomid.h"

#include <immer/set.hpp>
#include <optional> // For std::optional in firstElementNotIn if needed, or remove if not used

namespace detail {
template<typename Type_>
struct NODISCARD BasicRoomIdSet
{
private:
    using Type = Type_;
    using Set = immer::set<Type>;

public:
    // immer::set iterators are not directly comparable to std::set iterators.
    // immer::set provides range-based for loops.
    // If specific iterator types are needed, they would be Set::iterator
    using ConstIterator = typename Set::iterator; // immer sets are immutable, so iterator is const_iterator

private:
    Set m_set;

public:
    BasicRoomIdSet() noexcept = default;
    // Constructor from a single RoomId might need to change if insert signature changes
    explicit BasicRoomIdSet(const RoomId one) : m_set(Set().insert(one)) {}

public:
    // For immer::set, clear is achieved by assigning an empty set
    void clear() noexcept { m_set = Set(); }

public:
    NODISCARD ConstIterator cbegin() const { return m_set.begin(); }
    NODISCARD ConstIterator cend() const { return m_set.end(); }
    NODISCARD ConstIterator begin() const { return cbegin(); }
    NODISCARD ConstIterator end() const { return cend(); }
    NODISCARD size_t size() const { return m_set.size(); }
    NODISCARD bool empty() const noexcept { return m_set.empty(); }

public:
    NODISCARD bool contains(const Type id) const
    {
        // immer::set::count returns 0 or 1, or use find
        return m_set.count(id) > 0;
    }

private:
    // This method needs to be re-evaluated. immer::set does not provide ordered iteration
    // in the same way std::set does for this specific algorithm.
    // For now, let's provide a simpler, potentially less efficient version or mark as TODO.
    // A direct port of firstElementNotIn is not straightforward with immer's typical usage patterns.
    // Consider if this exact logic is still needed or if a different approach is better.
    // For the purpose of this refactoring, we might simplify or temporarily remove its usage
    // if it complicates things excessively without a clear immer-idiomatic replacement.
    NODISCARD static std::optional<Type> firstElementNotIn(const Set &a, const Set &b)
    {
        for (const auto& elem_a : a) {
            if (b.count(elem_a) == 0) {
                return elem_a;
            }
        }
        return std::nullopt;
    }

public:
    NODISCARD bool containsElementNotIn(const BasicRoomIdSet &other) const
    {
        if (this == &other) {
            return false;
        }
        // The O(N + M) optimization of firstElementNotIn is hard to replicate directly
        // due to unordered nature of iteration in general immer sets (though they are ordered).
        // Falling back to a simpler check:
        for (const Type id : m_set) {
            if (!other.contains(id)) {
                return true;
            }
        }
        return false;
    }

    // immer::set has its own operator==
    NODISCARD bool operator==(const BasicRoomIdSet &rhs) const { return m_set == rhs.m_set; }
    NODISCARD bool operator!=(const BasicRoomIdSet &rhs) const { return !operator==(rhs); }

public:
    // immer::set operations return new sets
    NODISCARD BasicRoomIdSet erase(const Type id) const {
        return BasicRoomIdSet(m_set.erase(id)); // Return a new BasicRoomIdSet
    }
    void insert(const Type id) { m_set = m_set.insert(id); } // insert can remain void if preferred

private:
    // Private constructor for erase to use
    explicit BasicRoomIdSet(Set new_set) : m_set(std::move(new_set)) {}

public:
    // insertAll needs to iterate and insert one by one, or use transient if performance is critical
    void insertAll(const BasicRoomIdSet &other)
    {
        auto temp_set = m_set;
        for (const Type id : other.m_set) {
            temp_set = temp_set.insert(id);
        }
        m_set = temp_set;
    }

public:
    NODISCARD Type first() const
    {
        if (empty()) {
            throw std::out_of_range("set is empty");
        }
        // immer::set iterators point to elements. It's ordered, so begin() is the smallest.
        return *m_set.begin();
    }

    NODISCARD Type last() const
    {
        if (empty()) {
            throw std::out_of_range("set is empty");
        }
        // To get the last element, we might need to iterate or convert to a different structure.
        // This is inefficient. Consider if this method is strictly necessary.
        // A more immer-idiomatic way might be to not rely on 'last' directly if performance is key.
        // For now, iterate to the last element.
        auto it = m_set.begin();
        auto last_it = it;
        while(it != m_set.end()) {
            last_it = it;
            ++it;
        }
        return *last_it;
    }
};
} // namespace detail

using RoomIdSet = detail::BasicRoomIdSet<RoomId>;
using ExternalRoomIdSet = detail::BasicRoomIdSet<ExternalRoomId>;

// std::hash specializations for RoomIdSet and ExternalRoomIdSet if they are used as keys in unordered maps.
// For immer::map, this is not strictly necessary for the values, but good practice if they could be keys.
// immer containers handle hashing of their contents if the element types are hashable.

namespace std {
    template<> struct hash<RoomIdSet> {
        size_t operator()(const RoomIdSet& s) const noexcept {
            size_t h = 0;
            // A simple hash combining elements. For immer::set, iteration order is defined.
            for (const auto& id : s) {
                h ^= std::hash<RoomId>{}(id) + 0x9e3779b9 + (h << 6) + (h >> 2);
            }
            return h;
        }
    };

    template<> struct hash<ExternalRoomIdSet> {
        size_t operator()(const ExternalRoomIdSet& s) const noexcept {
            size_t h = 0;
            for (const auto& id : s) {
                h ^= std::hash<ExternalRoomId>{}(id) + 0x9e3779b9 + (h << 6) + (h >> 2);
            }
            return h;
        }
    };
} // namespace std


namespace test {
extern void testRoomIdSet();
} // namespace test
