#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2024 The MMapper Authors

#include "macros.h"

#include <cstdint>
#if __has_include(<source_location>)
#include <source_location>
#endif

namespace mm {
#if defined(__cpp_lib_source_location) && __cpp_lib_source_location >= 201907L
using source_location = std::source_location;
#define MM_SOURCE_LOCATION() (std::source_location::current())
#else
struct NODISCARD source_location final
{
    const char *m_file_name = "";
    const char *m_function_name = "";
    std::uint_least32_t m_line = 0;

    NODISCARD constexpr const char *file_name() const { return this->m_file_name; }
    NODISCARD constexpr const char *function_name() const { return this->m_function_name; }
    NODISCARD constexpr std::uint_least32_t line() const { return this->m_line; }
};
#define MM_SOURCE_LOCATION() (mm::source_location{__FILE__, __FUNCTION__, __LINE__})
#endif
} // namespace mm
