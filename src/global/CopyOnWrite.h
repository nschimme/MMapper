#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2024 The MMapper Authors // Assuming year update is fine

#include "macros.h" // For NODISCARD etc.
#include <memory>
#include <variant>
#include <utility> // For std::move, std::holds_alternative, std::get
#include <stdexcept> // For std::runtime_error, std::invalid_argument
#include <type_traits> // For static_asserts
#include "utils.h"
#include "logging.h"

// Concept to ensure T is a non-reference, non-cv-qualified type
template<typename T>
concept Cowable = std::is_same_v<T, std::remove_cvref_t<T>>;

template<Cowable T>
class NODISCARD CopyOnWrite
{
private:
    using Mutable = std::shared_ptr<T>;
    using ReadOnly = std::shared_ptr<const T>;
    mutable std::variant<Mutable, ReadOnly> m_data;

public:
    // Default Constructor
    NODISCARD explicit CopyOnWrite()
        : CopyOnWrite{T{}} // Requires default constructor for T
    {
        MMLOG_DEBUG() << PRETTY_FUNCTION << " " << this;
    }

    // Constructor from const T&
    NODISCARD explicit CopyOnWrite(const T &data)
        : m_data{std::make_shared<typename Mutable::element_type>(data)} // Use Mutable::element_type for clarity
    {
        MMLOG_DEBUG() << PRETTY_FUNCTION << " " << this;
    }

    // Constructor from ReadOnly (std::shared_ptr<const T>)
    NODISCARD explicit CopyOnWrite(ReadOnly data)
        : m_data{std::move(data)}
    {
        MMLOG_DEBUG() << PRETTY_FUNCTION << " " << this;
        if (!std::get<ReadOnly>(m_data)) { // Check after move, or check data before move
            throw std::invalid_argument("ReadOnly data pointer must not be null.");
        }
    }

    // Copy Constructor
    NODISCARD CopyOnWrite(const CopyOnWrite &other)
    {
        MMLOG_DEBUG() << PRETTY_FUNCTION << " " << this << " from " << &(other);
        // Ensure the other object is in a shareable (read-only) state.
        // This call to makeReadOnly() is const but modifies other.m_data due to 'mutable'
        other.makeReadOnly();
        m_data = std::get<ReadOnly>(other.m_data); // Share the read-only data
    }

    // Copy Assignment Operator
    CopyOnWrite &operator=(const CopyOnWrite &other)
    {
        MMLOG_DEBUG() << PRETTY_FUNCTION << " " << this << " from " << &(other);
        if (this != &other) {
            // Ensure the other object is in a shareable (read-only) state.
            other.makeReadOnly();
            m_data = std::get<ReadOnly>(other.m_data); // Share the read-only data
        }
        return *this;
    }

    // Core Logic Methods
    NODISCARD bool isMutable() const {
        MMLOG_DEBUG() << PRETTY_FUNCTION << " " << this;
        return std::holds_alternative<Mutable>(m_data);
    }

    NODISCARD bool isReadOnly() const {
        MMLOG_DEBUG() << PRETTY_FUNCTION << " " << this;
        return std::holds_alternative<ReadOnly>(m_data);
    }

    // Should be const. Internally calls makeReadOnly() which is also const
    // but modifies m_data because m_data is mutable.
    NODISCARD const T &getReadOnly() const {
        MMLOG_DEBUG() << PRETTY_FUNCTION << " " << this;
        if (isMutable()) {
            makeReadOnly();
        }
        // After makeReadOnly, it must be ReadOnly, or if it already was.
        // The deref function will handle if the pointer is somehow null.
        return deref(std::get<ReadOnly>(m_data));
    }

    // Not const, as it may modify the state by copying.
    NODISCARD T &getMutable() {
        MMLOG_DEBUG() << PRETTY_FUNCTION << " " << this;
        if (isReadOnly()) {
            // Perform copy-on-write:
            // 1. Get current read-only data (which is a const T&)
            // 2. Construct a new CopyOnWrite from this const T&. This new Cow will hold a Mutable pointer to a copy.
            // 3. Assign this new Cow to *this.
            *this = CopyOnWrite{getReadOnly()}; // This re-uses the T& constructor.
                                                // getReadOnly() itself ensures 'this' becomes read-only before the copy.
                                                // Then the new object is move-assigned to *this.
            // After this operation, *this must hold mutable data.
            // Add an assertion here if project uses them commonly.
            // assert(isMutable());
        }
        // If it was already mutable, or after COW, it's mutable.
        // The deref function will handle if the pointer is somehow null (though logic should prevent this).
        return deref(std::get<Mutable>(m_data));
    }

    // This method is const, but it can modify m_data because m_data is declared mutable.
    // This is a key part of the pattern: logically const operations (like preparing for sharing)
    // might require internal state changes.
    void makeReadOnly() const {
        MMLOG_DEBUG() << PRETTY_FUNCTION << " " << this;
        if (isMutable()) {
            // Retrieve the mutable pointer, then store it back as a read-only pointer.
            // This shares the underlying data block but changes its access type in this Cow instance.
            m_data = static_cast<ReadOnly>(std::get<Mutable>(m_data));
        }
        // Add an assertion here if project uses them commonly.
        // assert(isReadOnly());
    }

    NODISCARD bool operator==(const CopyOnWrite<T> &other) const {
        MMLOG_DEBUG() << PRETTY_FUNCTION << " " << this << " from " << &(other); // Using existing logging macro for now
        const T* p1_ptr = nullptr;
        const T* p2_ptr = nullptr;

        // Visit the variant for the current object
        std::visit([&p1_ptr](const auto& ptr_variant) {
            if (ptr_variant) { // Check if shared_ptr itself is not null
                p1_ptr = ptr_variant.get(); // Get raw pointer
            }
        }, m_data);

        // Visit the variant for the 'other' object
        std::visit([&p2_ptr](const auto& ptr_variant) {
            if (ptr_variant) { // Check if shared_ptr itself is not null
                p2_ptr = ptr_variant.get(); // Get raw pointer
            }
        }, other.m_data);

        if (p1_ptr == p2_ptr) { // Handles both being nullptr or pointing to the exact same instance
            return true;
        }
        if (p1_ptr && p2_ptr) { // Both are non-nullptr, compare the pointed-to objects
            return *p1_ptr == *p2_ptr; // Requires T to have operator== defined
        }
        // One is nullptr and the other is not (or an unhandled case)
        return false;
    }

    NODISCARD bool operator!=(const CopyOnWrite<T> &other) const {
        MMLOG_DEBUG() << PRETTY_FUNCTION << " " << this << " from " << &(other);
        return !(*this == other);
    }

// Closing brace for the class will be below this
    // Destructor (defaulted)
    ~CopyOnWrite() noexcept = default;

    // Move constructor and assignment (defaulted)
    CopyOnWrite(CopyOnWrite &&other) noexcept = default;
    CopyOnWrite &operator=(CopyOnWrite &&other) noexcept = default;

    // Placeholder for other members to be added in subsequent steps
};

// Deduction Guides
template<Cowable T>
CopyOnWrite(T) -> CopyOnWrite<T>;

template<Cowable T>
CopyOnWrite(const CopyOnWrite<T>&) -> CopyOnWrite<T>; // More specific for lvalue Cow

template<Cowable T>
CopyOnWrite(std::shared_ptr<const T>) -> CopyOnWrite<T>;

template<Cowable T>
CopyOnWrite(std::shared_ptr<T>) -> CopyOnWrite<T>;
