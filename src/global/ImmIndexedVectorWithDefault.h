#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#include "ImmIndexedVector.h"
#include "NullPointerException.h"
#include "logging.h"
#include "macros.h"

template<typename ValueType_, typename IndexType_>
struct NODISCARD ImmIndexedVectorWithDefault
{
private:
    using ValueType = ValueType_;
    using IndexType = IndexType_;

private:
    ImmIndexedVector<ValueType_, IndexType_> m_vec;
    ValueType m_defaultValue;

public:
    explicit ImmIndexedVectorWithDefault()
        : ImmIndexedVectorWithDefault(ValueType{})
    {}

    explicit ImmIndexedVectorWithDefault(const ValueType defaultValue)
        : m_defaultValue{defaultValue}
    {}

public:
    NODISCARD size_t size() const { return m_vec.size(); }
    NODISCARD bool has(const IndexType id) const
    {
        const auto pos = id.value();
        return pos < size();
    }

public:
    void init(const ValueType *const data, const size_t size)
    {
        if (data == nullptr) {
            throw NullPointerException();
        }

        if (size == 0) {
            throw std::invalid_argument("size");
        }

        MMLOG() << "init " << size << " x " << sizeof(ValueType) << " = "
                << size * sizeof(ValueType) << " bytes";

        m_vec.init(data, size);
    }

public:
    void grow_to_size(const size_t want)
    {
        auto &vec = m_vec;
        const size_t have = vec.size();
        assert(have <= want);
        if (have < want) {
            // vec.reserve(want);
            for (size_t i = have; i < want; ++i) {
                vec.push_back(m_defaultValue);
            }
        }
        assert(size() == want);
    }

public:
    void grow_to_include(const size_t highestIndex) { grow_to_size(highestIndex + 1); }

public:
    void set(const IndexType id, const ValueType &value)
    {
        if (!has(id)) {
            throw std::runtime_error("out of bounds");
        }

        m_vec.set(id, value);
    }

    void grow_and_set(const IndexType id, const ValueType &value)
    {
        grow_to_include(id.value());
        set(id, value);
    }

public:
    NODISCARD const ValueType &at(const IndexType id) const
    {
        if (!has(id)) {
            throw std::runtime_error("out of bounds");
        }

        return deref(m_vec.find(id));
    }
    NODISCARD const ValueType &operator[](const IndexType id) const { return at(id); }

public:
    void removeAt(const IndexType id) { set(id, m_defaultValue); }

    void requireUninitialized(const IndexType id)
    {
        if (at(id) != m_defaultValue) {
            throw std::runtime_error("failed assertion");
        }
    }

public:
    NODISCARD bool operator==(const ImmIndexedVectorWithDefault &rhs) const
    {
        if (m_defaultValue != rhs.m_defaultValue) {
            assert(false); // Shouldn't be comparing two with different default values
            return false;
        }

        return m_vec == rhs.m_vec;
    }
    NODISCARD bool operator!=(const ImmIndexedVectorWithDefault &rhs) const
    {
        return !(rhs == *this);
    }

public:
    template<typename Callback>
    void for_each(Callback &&callback) const
    {
        m_vec.for_each(std::forward<Callback>(callback));
    }
};

namespace test {
void testIndexedVectorWithDefault();
} // namespace test
