#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2021-2023 The MMapper Authors & Jeff Plaisance

#include "../global/macros.h"
#include "roomid.h"

#include <bpptree/bpptree.hpp>   // Main BppTree header
#include <bpptree/ordered.hpp> // For BppTreeSet and Ordered mixin functionality

#include <optional>
#include <stdexcept> // For std::out_of_range
#include <algorithm> // For std::equal

namespace detail {
template<typename Type_>
struct NODISCARD BasicRoomIdSet
{
private:
    using Type = Type_;
    // Use BppTreeSet for COW semantics and ordered, unique key behavior.
    using InternalSet = typename bpptree::BppTreeSet<Type>::Persistent;

public:
    // Iterators from Persistent BppTreeSet are const_iterators.
    using ConstIterator = typename InternalSet::const_iterator;

private:
    InternalSet m_set;

    // Private constructor to create from an existing InternalSet (used by modifying methods)
    explicit BasicRoomIdSet(InternalSet new_set) : m_set(std::move(new_set)) {}

public:
    BasicRoomIdSet() noexcept = default; // Creates an empty persistent set

    // Constructor for a single element. RoomId specific one was there before.
    // Making it generic for Type.
    explicit BasicRoomIdSet(const Type one) : m_set(InternalSet().insert_v(one)) {}

public:
    // clear() returns a new, empty BasicRoomIdSet
    NODISCARD BasicRoomIdSet clear() const noexcept {
        return BasicRoomIdSet(); // Return a default-constructed (empty) set
    }

public:
    NODISCARD ConstIterator cbegin() const { return m_set.cbegin(); }
    NODISCARD ConstIterator cend() const { return m_set.cend(); }
    NODISCARD ConstIterator begin() const { return m_set.cbegin(); }
    NODISCARD ConstIterator end() const { return m_set.cend(); }

    NODISCARD size_t size() const { return m_set.size(); }
    NODISCARD bool empty() const noexcept { return m_set.empty(); }

public:
    NODISCARD bool contains(const Type id) const {
        // Uses BppTreeSet<Type>::Persistent::contains (from Ordered mixin), which is O(log N).
        return m_set.contains(id);
    }

private:
    // Helper for containsElementNotIn, uses iterators.
    // BppTree iterators are standard-compliant, allowing this O(N+M) comparison.
    NODISCARD static std::optional<Type> firstElementNotInDetail(const InternalSet &a, const InternalSet &b_set)
    {
        auto a_it = a.cbegin();
        const auto a_end = a.cend();
        auto b_it = b_set.cbegin();
        const auto b_end = b_set.cend();

        while (a_it != a_end) {
            const auto &x = *a_it;
            if (b_it == b_end) { // b_set is exhausted, x must be the first not in b_set
                return x;
            }
            const auto &y = *b_it; // Value from b_set
            if (x < y) { // x is smaller, so it's not in b_set (since b_set is sorted and we haven't reached x)
                return x;
            }
            if (!(y < x)) { // x == y, so x is in b_set. Advance a_it.
                ++a_it;
            }
            // If y < x, it means y is too small, advance b_it to catch up to x or pass it.
            // If x == y, b_it also needs to advance for the next comparison.
            ++b_it;
        }
        return std::nullopt;
    }

public:
    NODISCARD bool containsElementNotIn(const BasicRoomIdSet &other) const
    {
        if (this == &other) return false; // Same instance check.
        // TODO: BppTree might offer a more direct structural identity check for Persistent types
        //       (e.g., comparing root pointers if they are exposed or via a helper).
        //       For now, rely on operator== for content comparison if not the same instance.
        if (*this == other) return false; // If contents are equal, no element is "not in".

        return firstElementNotInDetail(m_set, other.m_set).has_value();
    }

    NODISCARD bool operator==(const BasicRoomIdSet &rhs) const {
        if (this == &rhs) return true; // Same instance check.
        if (m_set.size() != rhs.m_set.size()) {
            return false;
        }
        // BppTree's Persistent type might offer an optimized equality check,
        // especially if underlying structures are shared (structural equality).
        // If m_set had such a method (e.g. m_set.equals(rhs.m_set)), it could be used.
        // Otherwise, std::equal is the correct fallback for content comparison.
        return std::equal(m_set.begin(), m_set.end(), rhs.m_set.begin());
    }
    NODISCARD bool operator!=(const BasicRoomIdSet &rhs) const { return !operator==(rhs); }

public:
    NODISCARD BasicRoomIdSet erase(const Type id) const {
        // Uses BppTreeSet<Type>::Persistent::erase_key (from Ordered mixin), returns new Persistent set. O(log N).
        return BasicRoomIdSet(m_set.erase_key(id));
    }

    NODISCARD BasicRoomIdSet insert(const Type id) const {
        // Uses BppTreeSet<Type>::Persistent::insert_v (from Ordered mixin), returns new Persistent set.
        // DuplicatePolicy::ignore is used by default in the mixin, ensuring set semantics. O(log N).
        return BasicRoomIdSet(m_set.insert_v(id));
    }

public:
    NODISCARD BasicRoomIdSet insertAll(const BasicRoomIdSet &other) const {
        auto transient_set = m_set.transient();
        for (const Type& id : other.m_set) {
            transient_set.insert_v(id);
        }
        return BasicRoomIdSet(transient_set.persistent());
    }

public:
    NODISCARD Type first() const
    {
        if (empty()) {
            throw std::out_of_range("set is empty");
        }
        return m_set.front();
    }

    NODISCARD Type last() const
    {
        if (empty()) {
            throw std::out_of_range("set is empty");
        }
        return m_set.back();
    }
};
} // namespace detail

using RoomIdSet = detail::BasicRoomIdSet<RoomId>;
using ExternalRoomIdSet = detail::BasicRoomIdSet<ExternalRoomId>;

namespace test {
extern void testRoomIdSet();
} // namespace test
