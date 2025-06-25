#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2021-2024 The MMapper Authors & Jeff Plaisance

#include "../global/BppOrderedSet.h"
#include "roomid.h" // For RoomId and ExternalRoomId types

#include <optional> // For firstElementNotInDetail's return type
#include <stdexcept> // For out_of_range in first/last

// Forward declaration for ExternalRoomIdSet to be used if needed by RoomIdSet, and vice-versa
class ExternalRoomIdSet;

class NODISCARD RoomIdSet
{
private:
    BppOrderedSet<RoomId> m_data;

    // Private helper for containsElementNotIn, adapted from original BasicRoomIdSet
    // Takes const references to the underlying BppOrderedSet's InternalSet for direct bpp-tree iteration.
    NODISCARD static std::optional<RoomId> firstElementNotInDetail(
        const BppOrderedSet<RoomId>::InternalSet &a_tree,
        const BppOrderedSet<RoomId>::InternalSet &b_tree)
    {
        auto a_it = a_tree.cbegin();
        const auto a_end = a_tree.cend();
        auto b_it = b_tree.cbegin();
        const auto b_end = b_tree.cend();

        while (a_it != a_end) {
            const auto &x = *a_it;
            if (b_it == b_end) { // b_tree is exhausted, x must be the first not in b_tree
                return x;
            }
            const auto &y = *b_it; // Value from b_tree
            if (x < y) { // x is smaller, so it's not in b_tree
                return x;
            }
            if (!(y < x)) { // x == y, so x is in b_tree. Advance a_it.
                ++a_it;
            }
            // If y < x, it means y is too small, advance b_it to catch up to x or pass it.
            // If x == y, b_it also needs to advance for the next comparison.
            ++b_it;
        }
        return std::nullopt;
    }


public:
    // --- Typedefs ---
    using ValueType = RoomId;
    using ConstIterator = BppOrderedSet<RoomId>::ConstIterator; // Expose iterator type

    // --- Constructors ---
    RoomIdSet() = default;
    explicit RoomIdSet(RoomId id) : m_data(id) {}
    RoomIdSet(std::initializer_list<RoomId> ilist) : m_data(ilist) {}

    // Allow construction from the underlying BppOrderedSet if needed (e.g. for advanced interop)
    explicit RoomIdSet(const BppOrderedSet<RoomId>& underlying_set) : m_data(underlying_set) {}
    explicit RoomIdSet(BppOrderedSet<RoomId>&& underlying_set) : m_data(std::move(underlying_set)) {}


    // --- Forwarded Core Modifying Methods ---
    void clear() noexcept { m_data.clear(); }
    void insert(RoomId id) { m_data.insert(id); }
    void erase(RoomId id) { m_data.erase(id); }

    // --- RoomIdSet Specific Modifying Methods ---
    void insertAll(const RoomIdSet& other) {
        if (other.empty() || this == &other || m_data == other.m_data) {
            return;
        }
        if (this->empty()) {
            m_data = other.m_data;
            return;
        }
        // Accessing m_data.m_set directly is possible because m_set is public in BppOrderedSet struct.
        // Or, preferably, use a transient from one and insert from other's iterators.
        auto transient_tree = m_data.bpptreeSet().transient();
        for (RoomId id : other) { // Iterate RoomIdSet directly
            transient_tree.insert_v(id);
        }
        m_data.bpptreeSet() = transient_tree.persistent();
    }

    // --- Forwarded Core Non-Modifying Methods ---
    NODISCARD ConstIterator begin() const { return m_data.begin(); }
    NODISCARD ConstIterator end() const { return m_data.end(); }
    NODISCARD ConstIterator cbegin() const { return m_data.cbegin(); }
    NODISCARD ConstIterator cend() const { return m_data.cend(); }

    NODISCARD size_t size() const { return m_data.size(); }
    NODISCARD bool empty() const noexcept { return m_data.empty(); }
    NODISCARD bool contains(RoomId id) const { return m_data.contains(id); }

    // --- RoomIdSet Specific Non-Modifying Methods ---
    NODISCARD RoomId first() const {
        if (empty()) {
            throw std::out_of_range("RoomIdSet::first(): set is empty");
        }
        // BppOrderedSet does not have first()/last(), use its bpptreeSet().front()
        return m_data.bpptreeSet().front();
    }

    NODISCARD RoomId last() const {
        if (empty()) {
            throw std::out_of_range("RoomIdSet::last(): set is empty");
        }
        return m_data.bpptreeSet().back();
    }

    NODISCARD bool containsElementNotIn(const RoomIdSet& other) const {
        if (this == &other || m_data == other.m_data) return false;
        // Use the private helper, passing the internal bpp-trees
        return firstElementNotInDetail(m_data.bpptreeSet(), other.m_data.bpptreeSet()).has_value();
    }

    // --- Comparison Operators (forwarded) ---
    NODISCARD bool operator==(const RoomIdSet& rhs) const { return m_data == rhs.m_data; }
    NODISCARD bool operator!=(const RoomIdSet& rhs) const { return m_data != rhs.m_data; }

    // --- Access to underlying BppOrderedSet ---
    // Useful if some code really needs to operate on the generic BppOrderedSet<RoomId>
    NODISCARD const BppOrderedSet<RoomId>& asBppOrderedSet() const { return m_data; }
    BppOrderedSet<RoomId>& asBppOrderedSet() { return m_data; }
};


// --- ExternalRoomIdSet ---
// Defined similarly to RoomIdSet, wrapping BppOrderedSet<ExternalRoomId>
class NODISCARD ExternalRoomIdSet
{
private:
    BppOrderedSet<ExternalRoomId> m_data;

    NODISCARD static std::optional<ExternalRoomId> firstElementNotInDetail(
        const BppOrderedSet<ExternalRoomId>::InternalSet &a_tree,
        const BppOrderedSet<ExternalRoomId>::InternalSet &b_tree)
    {
        auto a_it = a_tree.cbegin();
        const auto a_end = a_tree.cend();
        auto b_it = b_tree.cbegin();
        const auto b_end = b_tree.cend();

        while (a_it != a_end) {
            const auto &x = *a_it;
            if (b_it == b_end) { return x; }
            const auto &y = *b_it;
            if (x < y) { return x; }
            if (!(y < x)) { ++a_it; }
            ++b_it;
        }
        return std::nullopt;
    }

public:
    using ValueType = ExternalRoomId;
    using ConstIterator = BppOrderedSet<ExternalRoomId>::ConstIterator;

    ExternalRoomIdSet() = default;
    explicit ExternalRoomIdSet(ExternalRoomId id) : m_data(id) {}
    ExternalRoomIdSet(std::initializer_list<ExternalRoomId> ilist) : m_data(ilist) {}
    explicit ExternalRoomIdSet(const BppOrderedSet<ExternalRoomId>& underlying_set) : m_data(underlying_set) {}
    explicit ExternalRoomIdSet(BppOrderedSet<ExternalRoomId>&& underlying_set) : m_data(std::move(underlying_set)) {}

    void clear() noexcept { m_data.clear(); }
    void insert(ExternalRoomId id) { m_data.insert(id); }
    void erase(ExternalRoomId id) { m_data.erase(id); }

    void insertAll(const ExternalRoomIdSet& other) {
        if (other.empty() || this == &other || m_data == other.m_data) return;
        if (this->empty()) { m_data = other.m_data; return; }
        auto transient_tree = m_data.bpptreeSet().transient();
        for (ExternalRoomId id : other) {
            transient_tree.insert_v(id);
        }
        m_data.bpptreeSet() = transient_tree.persistent();
    }

    NODISCARD ConstIterator begin() const { return m_data.begin(); }
    NODISCARD ConstIterator end() const { return m_data.end(); }
    NODISCARD ConstIterator cbegin() const { return m_data.cbegin(); }
    NODISCARD ConstIterator cend() const { return m_data.cend(); }

    NODISCARD size_t size() const { return m_data.size(); }
    NODISCARD bool empty() const noexcept { return m_data.empty(); }
    NODISCARD bool contains(ExternalRoomId id) const { return m_data.contains(id); }

    NODISCARD ExternalRoomId first() const {
        if (empty()) throw std::out_of_range("ExternalRoomIdSet::first(): set is empty");
        return m_data.bpptreeSet().front();
    }

    NODISCARD ExternalRoomId last() const {
        if (empty()) throw std::out_of_range("ExternalRoomIdSet::last(): set is empty");
        return m_data.bpptreeSet().back();
    }

    NODISCARD bool containsElementNotIn(const ExternalRoomIdSet& other) const {
        if (this == &other || m_data == other.m_data) return false;
        return firstElementNotInDetail(m_data.bpptreeSet(), other.m_data.bpptreeSet()).has_value();
    }

    NODISCARD bool operator==(const ExternalRoomIdSet& rhs) const { return m_data == rhs.m_data; }
    NODISCARD bool operator!=(const ExternalRoomIdSet& rhs) const { return m_data != rhs.m_data; }

    NODISCARD const BppOrderedSet<ExternalRoomId>& asBppOrderedSet() const { return m_data; }
    BppOrderedSet<ExternalRoomId>& asBppOrderedSet() { return m_data; }
};

namespace test {
// Declaration for the test function, implementation is in RoomIdSet.cpp
// This will test RoomIdSet, which now wraps BppOrderedSet.
extern void testRoomIdSet();
} // namespace test
