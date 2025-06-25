#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2021-2024 The MMapper Authors & Jeff Plaisance

#include "macros.h" // Adjusted path, assuming it's in the same global directory

#include <bpptree/bpptree.hpp>   // Main BppTree header
#include <bpptree/ordered.hpp> // For BppTreeSet and Ordered mixin functionality

#include <optional>
#include <stdexcept> // For std::out_of_range
#include <algorithm> // For std::equal

// Removed: #include "roomid.h" - BppOrderedSet should be generic

// Removed: namespace detail {
template<typename Type_>
struct NODISCARD BppOrderedSet // Renamed from BasicRoomIdSet
{
public: // Added public accessibility for ValueType
    using ValueType = Type_; // Standard alias for the type of elements
private:
    // using Type = Type_; // This alias is now ValueType
    // Use BppTreeSet for COW semantics and ordered, unique key behavior.
    using InternalSet = typename bpptree::BppTreeSet<ValueType>::Persistent;

public:
    // Iterators from Persistent BppTreeSet are const_iterators.
    using ConstIterator = typename InternalSet::const_iterator;

private:
    InternalSet m_set;

    // Private constructor to create from an existing InternalSet (used by modifying methods)
    explicit BppOrderedSet(InternalSet new_set) : m_set(std::move(new_set)) {}

public:
    BppOrderedSet() noexcept = default; // Creates an empty persistent set

    // Constructor for a single element.
    explicit BppOrderedSet(const ValueType one) : m_set(InternalSet().insert_v(one)) {}

    // Constructor from an existing bpptree set (e.g. for interop or advanced usage)
    explicit BppOrderedSet(const InternalSet &other_bpp_set) : m_set(other_bpp_set) {}
    explicit BppOrderedSet(InternalSet &&other_bpp_set) : m_set(std::move(other_bpp_set)) {}

    // Assignment operators for underlying set type
    BppOrderedSet &operator=(const InternalSet &other_bpp_set)
    {
        m_set = other_bpp_set;
        return *this;
    }
    BppOrderedSet &operator=(InternalSet &&other_bpp_set)
    {
        m_set = std::move(other_bpp_set);
        return *this;
    }


public:
    // clear() returns a new, empty BppOrderedSet
    NODISCARD BppOrderedSet clear() const noexcept {
        return BppOrderedSet(); // Return a default-constructed (empty) set
    }

public:
    NODISCARD ConstIterator cbegin() const { return m_set.cbegin(); }
    NODISCARD ConstIterator cend() const { return m_set.cend(); }
    NODISCARD ConstIterator begin() const { return m_set.cbegin(); } // Common alias for cbegin
    NODISCARD ConstIterator end() const { return m_set.cend(); }     // Common alias for cend

    NODISCARD size_t size() const { return m_set.size(); }
    NODISCARD bool empty() const noexcept { return m_set.empty(); }

public:
    NODISCARD bool contains(const ValueType id) const {
        return m_set.contains(id);
    }

private:
    // Helper for containsElementNotIn, uses iterators.
    NODISCARD static std::optional<ValueType> firstElementNotInDetail(const InternalSet &a, const InternalSet &b_set)
    {
        auto a_it = a.cbegin();
        const auto a_end = a.cend();
        auto b_it = b_set.cbegin();
        const auto b_end = b_set.cend();

        while (a_it != a_end) {
            const auto &x = *a_it;
            if (b_it == b_end) {
                return x;
            }
            const auto &y = *b_it;
            if (x < y) {
                return x;
            }
            if (!(y < x)) { // x == y
                ++a_it;
            }
            ++b_it;
        }
        return std::nullopt;
    }

public:
    NODISCARD bool containsElementNotIn(const BppOrderedSet &other) const
    {
        if (this == &other) return false;
        if (m_set == other.m_set) return false; // B+ Tree persistent sets are comparable directly
        return firstElementNotInDetail(m_set, other.m_set).has_value();
    }

    NODISCARD bool operator==(const BppOrderedSet &rhs) const {
        // Persistent B+ trees with COW often have efficient equality:
        // if root pointers are same, they are equal.
        // The bpptree::BppTreeSet::Persistent itself should define operator==
        return m_set == rhs.m_set;
    }
    NODISCARD bool operator!=(const BppOrderedSet &rhs) const { return !operator==(rhs); }

public:
    NODISCARD BppOrderedSet erase(const ValueType id) const {
        return BppOrderedSet(m_set.erase_key(id));
    }

    NODISCARD BppOrderedSet insert(const ValueType id) const {
        return BppOrderedSet(m_set.insert_v(id));
    }

public:
    NODISCARD BppOrderedSet insertAll(const BppOrderedSet &other) const {
        if (other.empty()) return *this; // Optimization: if other is empty, return current set
        if (this->empty()) return other; // Optimization: if current is empty, return other set

        auto transient_set = m_set.transient(); // Create a transient copy for efficient batch modification
        for (const ValueType& id : other.m_set) { // Iterate over the internal bpp-tree of 'other'
            transient_set.insert_v(id); // insert_v handles duplicates by ignoring them (set semantics)
        }
        return BppOrderedSet(transient_set.persistent()); // Convert back to persistent
    }

public:
    NODISCARD ValueType first() const
    {
        if (empty()) {
            throw std::out_of_range("BppOrderedSet::first(): set is empty");
        }
        // BppTreeSet::Persistent provides front()
        return m_set.front();
    }

    NODISCARD ValueType last() const
    {
        if (empty()) {
            throw std::out_of_range("BppOrderedSet::last(): set is empty");
        }
        // BppTreeSet::Persistent provides back()
        return m_set.back();
    }

    // Provide access to the underlying bpptree set if needed for advanced operations or interoperability
    NODISCARD const InternalSet& bpptreeSet() const { return m_set; }
};
// Removed: using RoomIdSet = detail::BasicRoomIdSet<RoomId>;
// Removed: using ExternalRoomIdSet = detail::BasicRoomIdSet<ExternalRoomId>;
// Removed: namespace test { extern void testRoomIdSet(); }
// Removed: } // namespace detail
