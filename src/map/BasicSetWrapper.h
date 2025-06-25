#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2024 The MMapper Authors

#include "../global/BppOrderedSet.h" // The lean generic BppOrderedSet struct

#include <optional>  // For std::optional (return type of firstElementNotInDetail)
#include <stdexcept> // For std::out_of_range (used in first/last)
#include <initializer_list>

// BasicSetWrapper is a template class that wraps BppOrderedSet<WrappedValueType>
// and adds more specialized set operations like insertAll, first, last,
// and containsElementNotIn, making them available generically.
template<typename WrappedValueType>
class NODISCARD BasicSetWrapper
{
private:
    BppOrderedSet<WrappedValueType> m_data;

    // Private helper for containsElementNotIn.
    // Operates on the underlying BppOrderedSet's InternalSet (bpp-tree) for efficiency.
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
            if (b_it == b_end) { // b_tree is exhausted, x must be the first not in b_tree
                return x;
            }
            const auto &y = *b_it; // Value from b_tree
            if (x < y) { // x is smaller, so it's not in b_tree (as b_tree is sorted)
                return x;
            }
            if (!(y < x)) { // x == y, so x is in b_tree. Advance a_it.
                ++a_it;
            }
            // If y < x, it means y is too small from b_tree, advance b_it to catch up.
            // If x == y, b_it also needs to advance for the next comparison.
            ++b_it;
        }
        return std::nullopt; // All elements of 'a_tree' are found in 'b_tree'
    }

public:
    // --- Typedefs ---
    using ValueType = WrappedValueType;
    using ConstIterator = typename BppOrderedSet<WrappedValueType>::ConstIterator;

    // --- Constructors ---
    BasicSetWrapper() = default;
    explicit BasicSetWrapper(WrappedValueType val) : m_data(val) {}
    BasicSetWrapper(std::initializer_list<WrappedValueType> ilist) : m_data(ilist) {}

    // Construct/assign from the underlying BppOrderedSet
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

    // --- Forwarded Core Modifying Methods (void-returning) ---
    void clear() noexcept { m_data.clear(); }
    void insert(const ValueType& val) { m_data.insert(val); }
    void erase(const ValueType& val) { m_data.erase(val); }

    // --- Specialized Modifying Methods for BasicSetWrapper ---
    void insertAll(const BasicSetWrapper& other) {
        if (other.empty() || this == &other || m_data == other.m_data) {
            return; // No change if other is empty, or same instance, or identical content
        }
        if (this->empty()) {
            m_data = other.m_data; // Efficiently copy if current is empty
            return;
        }

        // Use BppOrderedSet's non-const bpptreeSet() to get access for transient optimization
        auto transient_tree = m_data.bpptreeSet().transient();
        // Iterate 'other' (which is BasicSetWrapper) directly.
        // BasicSetWrapper needs begin()/end() to be iterable.
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
        // BppOrderedSet's m_set is public, or use bpptreeSet()
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
            return false; // Same instance or identical content means no element is "not in".
        }
        // Use the private static helper, passing the internal bpp-trees via bpptreeSet()
        return firstElementNotInDetail(m_data.bpptreeSet(), other.m_data.bpptreeSet()).has_value();
    }

    // --- Comparison Operators (forwarded) ---
    NODISCARD bool operator==(const BasicSetWrapper& rhs) const { return m_data == rhs.m_data; }
    NODISCARD bool operator!=(const BasicSetWrapper& rhs) const { return !(operator==(rhs)); }

    // --- Access to underlying BppOrderedSet ---
    // Useful if some code really needs to operate on the generic BppOrderedSet
    NODISCARD const BppOrderedSet<WrappedValueType>& asBppOrderedSet() const { return m_data; }
    BppOrderedSet<WrappedValueType>& asBppOrderedSet() { return m_data; }
};
