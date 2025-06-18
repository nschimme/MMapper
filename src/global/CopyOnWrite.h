#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#include "logging.h"
#include "macros.h"
#include "utils.h"

#include <memory>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <variant>

template<typename T>
using remove_cvref_t = typename std::remove_cv<typename std::remove_reference<T>::type>::type;

template<typename T>
struct is_cowable
    : std::conjunction<std::is_same<T, remove_cvref_t<T>>, std::is_copy_constructible<T>>
{};

template<typename T>
inline constexpr bool is_cowable_v = is_cowable<T>::value;

template<typename T, typename = std::enable_if_t<is_cowable_v<T>>>
class NODISCARD CopyOnWrite
{
private:
    using Mutable = std::shared_ptr<T>;
    using ReadOnly = std::shared_ptr<const T>;
    mutable std::variant<Mutable, ReadOnly> m_data;

public:
    // Default Constructor requires default constructible T
    NODISCARD explicit CopyOnWrite()
        : CopyOnWrite{T{}} // delegates to CopyOnWrite(const T&)
    {
        // static_assert in delegated ctor ensures cowable
        //MMLOG_DEBUG() << PRETTY_FUNCTION << " " << this;
    }

    // Constructor from const T&
    NODISCARD explicit CopyOnWrite(const T &data)
        : m_data{std::make_shared<T>(data)}
    {
        static_assert(is_cowable_v<T>,
                      "CopyOnWrite<T>: T must be non-reference, non-cv-qualified type.");
        //MMLOG_DEBUG() << PRETTY_FUNCTION << " " << this;
    }

    // Constructor from ReadOnly (std::shared_ptr<const T>)
    NODISCARD explicit CopyOnWrite(ReadOnly data)
        : m_data{std::move(data)}
    {
        static_assert(is_cowable_v<T>,
                      "CopyOnWrite<T>: T must be non-reference, non-cv-qualified type.");
        //MMLOG_DEBUG() << PRETTY_FUNCTION << " " << this;
        if (!std::get<ReadOnly>(m_data)) {
            throw std::invalid_argument("ReadOnly data pointer must not be null.");
        }
    }

    // Copy Constructor
    NODISCARD CopyOnWrite(const CopyOnWrite &other)
    {
        static_assert(is_cowable_v<T>,
                      "CopyOnWrite<T>: T must be non-reference, non-cv-qualified type.");
        //MMLOG_DEBUG() << PRETTY_FUNCTION << " " << this << " from " << &other;
        other.makeReadOnly();
        m_data = std::get<ReadOnly>(other.m_data);
    }

    // Copy Assignment Operator
    CopyOnWrite &operator=(const CopyOnWrite &other)
    {
        static_assert(is_cowable_v<T>,
                      "CopyOnWrite<T>: T must be non-reference, non-cv-qualified type.");
        //MMLOG_DEBUG() << PRETTY_FUNCTION << " " << this << " from " << &other;
        if (this != &other) {
            other.makeReadOnly();
            m_data = std::get<ReadOnly>(other.m_data);
        }
        return *this;
    }

    // Move constructor and assignment
    DEFAULT_MOVE_CTOR(CopyOnWrite);
    DEFAULT_MOVE_ASSIGN_OP(CopyOnWrite);

    DTOR(CopyOnWrite) noexcept = default;

    // Check if current data is mutable
    NODISCARD bool isMutable() const
    {
        //MMLOG_DEBUG() << PRETTY_FUNCTION << " " << this;
        return std::holds_alternative<Mutable>(m_data);
    }

    // Check if current data is read-only
    NODISCARD bool isReadOnly() const
    {
        //MMLOG_DEBUG() << PRETTY_FUNCTION << " " << this;
        return std::holds_alternative<ReadOnly>(m_data);
    }

    // Get const ref to underlying T
    NODISCARD const T &getReadOnly() const
    {
        //MMLOG_DEBUG() << PRETTY_FUNCTION << " " << this;
        if (isMutable()) {
            makeReadOnly();
        }
        return *std::get<ReadOnly>(m_data);
    }

    // Get mutable ref to underlying T (trigger copy if needed)
    NODISCARD T &getMutable()
    {
        //MMLOG_DEBUG() << PRETTY_FUNCTION << " " << this;
        if (isReadOnly()) {
            // Copy underlying const data to new mutable shared_ptr
            *this = CopyOnWrite{getReadOnly()};
        }
        return *std::get<Mutable>(m_data);
    }

    // Make current data read-only (shared)
    void makeReadOnly() const
    {
        //MMLOG_DEBUG() << PRETTY_FUNCTION << " " << this;
        if (isMutable()) {
            m_data = std::static_pointer_cast<const T>(std::get<Mutable>(m_data));
        }
    }

    // Equality operators compare pointed-to values
    NODISCARD bool operator==(const CopyOnWrite<T> &other) const
    {
        //MMLOG_DEBUG() << PRETTY_FUNCTION << " " << this << " vs " << &other;

        const T *p1_ptr = nullptr;
        const T *p2_ptr = nullptr;

        std::visit([&p1_ptr](const auto &ptr) { p1_ptr = ptr.get(); }, m_data);

        std::visit([&p2_ptr](const auto &ptr) { p2_ptr = ptr.get(); }, other.m_data);

        if (p1_ptr == p2_ptr)
            return true; // same shared object

        if (p1_ptr && p2_ptr)
            return *p1_ptr == *p2_ptr;

        return false; // one or both null
    }

    NODISCARD bool operator!=(const CopyOnWrite<T> &other) const { return !(*this == other); }
};

template<typename T>
CopyOnWrite(T) -> CopyOnWrite<T>;

template<typename T>
CopyOnWrite(const CopyOnWrite<T> &) -> CopyOnWrite<T>;

template<typename T>
CopyOnWrite(std::shared_ptr<const T>) -> CopyOnWrite<T>;
