// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "Value.h"

#include "../global/Consts.h"
#include "../global/PrintUtils.h"
#include "../global/TextUtils.h"

#include <cassert>
#include <cstdint>
#include <iomanip>
#include <memory>
#include <ostream>
#include <vector>

namespace value_helper {

static void print(std::ostream &os, bool value)
{
    if ((false)) {
        os << std::boolalpha << value << std::noboolalpha;
    } else {
        os << (value ? "true" : "false");
    }
}

static void print(std::ostream &os, char value)
{
    os << char_consts::C_SQUOTE;
    ::print_char(os, value, false);
    os << char_consts::C_SQUOTE;
}

static void print(std::ostream &os, int32_t value)
{
    os << value;
}

static void print(std::ostream &os, int64_t value)
{
    // The "long" here doesn't refer to the C++ keyword `long`; instead, it's there to tell humans
    // (who may not even know the code is written in C++) that it's a 64-bit value.
    os << "long(" << value << ")";
}

static void print(std::ostream &os, float value)
{
    os << "float(" << value << ")";
}

static void print(std::ostream &os, double value)
{
    os << "double(" << value << ")";
}

static void print(std::ostream &os, const std::string &value)
{
    ::print_string_quoted(os, value);
}

static void print(std::ostream &os, const Vector &value)
{
    os << value;
}

static void print(std::ostream &os, DoorFlagEnum value)
{
    os << "DoorFlag(" << to_string_view(value) << ")";
}

static void print(std::ostream &os, ExitFlagEnum value)
{
    os << "ExitFlag(" << to_string_view(value) << ")";
}

static void print(std::ostream &os, ExitDirEnum value)
{
    os << "Direction(" << to_string_view(value) << ")";
}

static void print(std::ostream &os, InfomarkClassEnum value)
{
    os << "InfomarkClassEnum(" << static_cast<int>(value) << ")";
}

} // namespace value_helper

std::ostream &operator<<(std::ostream &os, const Value &value)
{
#define X_CASE(Type, RefType, CamelCase) \
    case Value::IndexEnum::CamelCase: { \
        value_helper::print(os, value.get##CamelCase()); \
        return os; \
    }

    switch (value.getType()) {
    case Value::IndexEnum::Null:
        return os << "null";
        //
        XFOREACH_VALUE_TYPE(X_CASE)
    }

    std::abort();
#undef X_CASE
}

std::ostream &operator<<(std::ostream &os, const Vector &v)
{
    os << "'["; // "Vector(";
    bool first = true;
    for (const Value &a : v) {
        if (!first) {
            os << ", ";
        }
        os << a;
        first = false;
    }
    return os << "]"; // ")";
}

Vector::Vector()
    : m_vector(std::make_shared<const Base>())
{}

Vector::Vector(Vector::Base &&x)
    : m_vector(std::make_shared<const Base>(std::move(x)))
{}

Vector::Base::const_iterator Vector::begin() const
{
    return m_vector->begin();
}

Vector::Base::const_iterator Vector::end() const
{
    return m_vector->end();
}

Vector getAnyVectorReversed(const Pair *const matched)
{
    size_t len = 0;
    for (const Pair *it = matched; it != nullptr; it = it->cdr) {
        ++len;
    }

    std::vector<Value> result;
    result.resize(len);

    auto out = result.rbegin();
    for (const Pair *it = matched; it != nullptr; it = it->cdr) {
        *out++ = it->car;
    }
    assert(out == result.rend());

    return Vector(std::move(result));
}
