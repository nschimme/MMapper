#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2021-2024 The MMapper Authors & Jeff Plaisance

#include "macros.h"

#include <bpptree/bpptree.hpp>
#include <bpptree/ordered.hpp>

#include <initializer_list>
#include <stdexcept> // For std::out_of_range (used by BppTree's front/back if they were here)
#include <algorithm> // For std::equal (though bpptree::Persistent might have its own ==)

// BppOrderedSet is a lean struct wrapping bpptree::BppTreeSet::Persistent.
// It provides essential set operations, modifying the set in-place by
// reassigning the internal persistent tree. It's intended as a generic
// building block for more specialized set classes.
template<typename Type_>
struct BppOrderedSet
{
public:
    using ValueType = Type_;
    using InternalSet = typename bpptree::BppTreeSet<ValueType>::Persistent;
    using ConstIterator = typename InternalSet::const_iterator;

    InternalSet m_set; // Public member for flexibility by wrapping classes

public:
    // --- Constructors ---
    BppOrderedSet() noexcept = default;

    explicit BppOrderedSet(const ValueType& one) : m_set(InternalSet().insert_v(one)) {}

    BppOrderedSet(std::initializer_list<ValueType> ilist) : m_set() {
        if (ilist.size() > 0) {
            auto transient_set = m_set.transient();
            for (const auto& item : ilist) {
                transient_set.insert_v(item);
            }
            m_set = transient_set.persistent();
        }
    }

    explicit BppOrderedSet(const InternalSet &other_bpp_set) : m_set(other_bpp_set) {}
    explicit BppOrderedSet(InternalSet &&other_bpp_set) : m_set(std::move(other_bpp_set)) {}

    // --- Assignment Operators ---
    BppOrderedSet& operator=(const InternalSet &other_bpp_set) {
        m_set = other_bpp_set;
        return *this;
    }
    BppOrderedSet& operator=(InternalSet &&other_bpp_set) {
        m_set = std::move(other_bpp_set);
        return *this;
    }
    BppOrderedSet& operator=(std::initializer_list<ValueType> ilist) {
        m_set = InternalSet();
        if (ilist.size() > 0) {
            auto transient_set = m_set.transient();
            for (const auto& item : ilist) {
                transient_set.insert_v(item);
            }
            m_set = transient_set.persistent();
        }
        return *this;
    }

    // --- Modifying Operations ---
    void clear() noexcept {
        m_set = InternalSet();
    }

    void insert(const ValueType& id) {
        m_set = m_set.insert_v(id);
    }

    void erase(const ValueType& id) {
        m_set = m_set.erase_key(id);
    }

    // --- Non-modifying (const) Operations ---
    NODISCARD ConstIterator cbegin() const { return m_set.cbegin(); }
    NODISCARD ConstIterator cend() const { return m_set.cend(); }
    NODISCARD ConstIterator begin() const { return m_set.cbegin(); }
    NODISCARD ConstIterator end() const { return m_set.cend(); }

    NODISCARD size_t size() const { return m_set.size(); }
    NODISCARD bool empty() const noexcept { return m_set.empty(); }

    NODISCARD bool contains(const ValueType& id) const {
        return m_set.contains(id);
    }

    // --- Comparison Operators ---
    NODISCARD bool operator==(const BppOrderedSet &rhs) const {
        return m_set == rhs.m_set;
    }
    NODISCARD bool operator!=(const BppOrderedSet &rhs) const {
        return !(operator==(rhs));
    }

    // --- Access to underlying bpptree set ---
    NODISCARD const InternalSet& bpptreeSet() const { return m_set; }
    InternalSet& bpptreeSet() { return m_set; }

    // Methods intentionally EXCLUDED: insertAll, containsElementNotIn, first, last
    // BppTree's m_set.front() and m_set.back() can be accessed via bpptreeSet() if needed by wrappers.
};
