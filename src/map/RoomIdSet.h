#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2021-2024 The MMapper Authors & Jeff Plaisance

#include "../global/BppOrderedSet.h" // The lean generic BppOrderedSet struct
#include "roomid.h"                  // For RoomId and ExternalRoomId types

#include <optional>  // For std::optional
#include <stdexcept> // For std::out_of_range
#include <initializer_list>

// BasicRoomIdSet is a template class defined directly in this file.
// It was previously named BasicSetWrapper in earlier design iterations.
// It wraps BppOrderedSet<WrappedValueType> and adds more specialized set operations
// like insertAll, first, last, and containsElementNotIn, making them available generically.
template<typename WrappedValueType>
class NODISCARD BasicRoomIdSet // Renamed from BasicSetWrapper
{
private:
    BppOrderedSet<WrappedValueType> m_data;

    // Private helper for containsElementNotIn.
    NODISCARD static std::optional<WrappedValueType> firstElementNotInDetail(
        const typename BppOrderedSet<WrappedValueType>::InternalSet &a_tree,
        const typename BppOrderedSet<WrappedValueType>::InternalSet &b_tree)
    {
        auto a_it = a_tree.cbegin();
        const auto a_end = a_tree.cend();
        auto b_it = b_tree.cbegin();
        const auto b_end = b_tree.cend();

        while (a_it != a_end) {
            const auto &x = *a_it;
            if (b_it == b_end) {
                return x;
            }
            const auto &y = *b_it;
            if (x < y) {
                return x;
            }
            if (!(y < x)) {
                ++a_it;
            }
            ++b_it;
        }
        return std::nullopt;
    }

public:
    // --- Typedefs ---
    using ValueType = WrappedValueType;
    using ConstIterator = typename BppOrderedSet<WrappedValueType>::ConstIterator;

    // --- Constructors ---
    BasicRoomIdSet() = default;
    explicit BasicRoomIdSet(WrappedValueType val) : m_data(val) {}
    BasicRoomIdSet(std::initializer_list<WrappedValueType> ilist) : m_data(ilist) {}

    explicit BasicRoomIdSet(const BppOrderedSet<WrappedValueType>& underlying_set) : m_data(underlying_set) {}
    explicit BasicRoomIdSet(BppOrderedSet<WrappedValueType>&& underlying_set) : m_data(std::move(underlying_set)) {}

    BasicRoomIdSet& operator=(const BppOrderedSet<WrappedValueType>& underlying_set) {
        m_data = underlying_set;
        return *this;
    }
    BasicRoomIdSet& operator=(BppOrderedSet<WrappedValueType>&& underlying_set) {
        m_data = std::move(underlying_set);
        return *this;
    }
     BasicRoomIdSet& operator=(std::initializer_list<WrappedValueType> ilist) {
        m_data = ilist;
        return *this;
    }

    // --- Forwarded Core Modifying Methods (void-returning) ---
    void clear() noexcept { m_data.clear(); }
    void insert(const ValueType& val) { m_data.insert(val); }
    void erase(const ValueType& val) { m_data.erase(val); }

    // --- Specialized Modifying Methods ---
    void insertAll(const BasicRoomIdSet& other) { // Takes const BasicRoomIdSet&
        if (other.empty() || this == &other || m_data == other.m_data) {
            return;
        }
        if (this->empty()) {
            m_data = other.m_data;
            return;
        }

        auto transient_tree = m_data.bpptreeSet().transient();
        // Iterate 'other' (which is BasicRoomIdSet) directly.
        for (const ValueType& val : other) { // Relies on BasicRoomIdSet having begin()/end()
            transient_tree.insert_v(val);
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
    NODISCARD bool contains(const ValueType& val) const { return m_data.contains(val); }

    // --- Specialized Non-Modifying Methods ---
    NODISCARD ValueType first() const {
        if (empty()) {
            throw std::out_of_range("BasicRoomIdSet::first(): set is empty");
        }
        return m_data.bpptreeSet().front();
    }

    NODISCARD ValueType last() const {
        if (empty()) {
            throw std::out_of_range("BasicRoomIdSet::last(): set is empty");
        }
        return m_data.bpptreeSet().back();
    }

    NODISCARD bool containsElementNotIn(const BasicRoomIdSet& other) const { // Takes const BasicRoomIdSet&
        if (this == &other || m_data == other.m_data) {
            return false;
        }
        return firstElementNotInDetail(m_data.bpptreeSet(), other.m_data.bpptreeSet()).has_value();
    }

    // --- Comparison Operators (forwarded) ---
    NODISCARD bool operator==(const BasicRoomIdSet& rhs) const { return m_data == rhs.m_data; } // Takes const BasicRoomIdSet&
    NODISCARD bool operator!=(const BasicRoomIdSet& rhs) const { return !(operator==(rhs)); }    // Takes const BasicRoomIdSet&

    // --- Access to underlying BppOrderedSet ---
    NODISCARD const BppOrderedSet<WrappedValueType>& asBppOrderedSet() const { return m_data; }
    BppOrderedSet<WrappedValueType>& asBppOrderedSet() { return m_data; }
};

// --- Type Aliases ---
// RoomIdSet is BasicRoomIdSet specialized with RoomId.
using RoomIdSet = BasicRoomIdSet<RoomId>;

// ExternalRoomIdSet is BasicRoomIdSet specialized with ExternalRoomId.
using ExternalRoomIdSet = BasicRoomIdSet<ExternalRoomId>;

// --- Test Declaration ---
namespace test {
// Declaration for the test function, implementation is in RoomIdSet.cpp.
// This will test BasicRoomIdSet<RoomId> via the RoomIdSet alias.
extern void testRoomIdSet();
} // namespace test
