#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2021-2024 The MMapper Authors & Jeff Plaisance

#include "../global/BppOrderedSet.h" // The lean generic BppOrderedSet struct
#include "roomid.h"                  // For RoomId and ExternalRoomId types

#include <optional>  // For std::optional
#include <stdexcept> // For std::out_of_range
#include <initializer_list>

// BasicSetWrapper is a template class defined directly in this file.
// It wraps BppOrderedSet<WrappedValueType> and adds more specialized set operations
// like insertAll, first, last, and containsElementNotIn, making them available generically.
template<typename WrappedValueType>
class NODISCARD BasicSetWrapper
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
    BasicSetWrapper() = default;
    explicit BasicSetWrapper(WrappedValueType val) : m_data(val) {}
    BasicSetWrapper(std::initializer_list<WrappedValueType> ilist) : m_data(ilist) {}

    explicit BasicSetWrapper(const BppOrderedSet<WrappedValueType>& underlying_set) : m_data(underlying_set) {}
    explicit BasicSetWrapper(BppOrderedSet<WrappedValueType>&& underlying_set) : m_data(std::move(underlying_set)) {}

    BasicSetWrapper& operator=(const BppOrderedSet<WrappedValueType>& underlying_set) {
        m_data = underlying_set;
        return *this;
    }
    BasicSetWrapper& operator=(BppOrderedSet<WrappedValueType>&& underlying_set) {
        m_data = std::move(underlying_set);
        return *this;
    }
    // operator= for initializer_list will rely on BppOrderedSet's operator= or implicit conversion
     BasicSetWrapper& operator=(std::initializer_list<WrappedValueType> ilist) {
        m_data = ilist; // Relies on BppOrderedSet::operator=(std::initializer_list)
        return *this;
    }


    // --- Forwarded Core Modifying Methods (void-returning) ---
    void clear() noexcept { m_data.clear(); }
    void insert(const ValueType& val) { m_data.insert(val); }
    void erase(const ValueType& val) { m_data.erase(val); }

    // --- Specialized Modifying Methods for BasicSetWrapper ---
    void insertAll(const BasicSetWrapper& other) {
        if (other.empty() || this == &other || m_data == other.m_data) {
            return;
        }
        if (this->empty()) {
            m_data = other.m_data;
            return;
        }

        auto transient_tree = m_data.bpptreeSet().transient();
        for (const ValueType& val : other) {
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

    // --- Specialized Non-Modifying Methods for BasicSetWrapper ---
    NODISCARD ValueType first() const {
        if (empty()) {
            throw std::out_of_range("BasicSetWrapper::first(): set is empty");
        }
        return m_data.bpptreeSet().front();
    }

    NODISCARD ValueType last() const {
        if (empty()) {
            throw std::out_of_range("BasicSetWrapper::last(): set is empty");
        }
        return m_data.bpptreeSet().back();
    }

    NODISCARD bool containsElementNotIn(const BasicSetWrapper& other) const {
        if (this == &other || m_data == other.m_data) {
            return false;
        }
        return firstElementNotInDetail(m_data.bpptreeSet(), other.m_data.bpptreeSet()).has_value();
    }

    // --- Comparison Operators (forwarded) ---
    NODISCARD bool operator==(const BasicSetWrapper& rhs) const { return m_data == rhs.m_data; }
    NODISCARD bool operator!=(const BasicSetWrapper& rhs) const { return !(operator==(rhs)); }

    // --- Access to underlying BppOrderedSet ---
    NODISCARD const BppOrderedSet<WrappedValueType>& asBppOrderedSet() const { return m_data; }
    BppOrderedSet<WrappedValueType>& asBppOrderedSet() { return m_data; }
};

// --- Type Aliases ---
// RoomIdSet is BasicSetWrapper specialized with RoomId.
using RoomIdSet = BasicSetWrapper<RoomId>;

// ExternalRoomIdSet is BasicSetWrapper specialized with ExternalRoomId.
using ExternalRoomIdSet = BasicSetWrapper<ExternalRoomId>;

// --- Test Declaration ---
namespace test {
// Declaration for the test function, implementation is in RoomIdSet.cpp.
// This will test BasicSetWrapper<RoomId> via the RoomIdSet alias.
extern void testRoomIdSet();
} // namespace test
