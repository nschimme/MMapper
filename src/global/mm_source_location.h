#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2024 The MMapper Authors

#include "macros.h"

#include <source_location>

namespace mm {
/*
 * NOTE: std::source_location (C++20) support:
 * - GCC 11+
 * - Clang 16+
 * - MSVC 19.29+ (Visual Studio 2019 16.11+)
 * - Apple Clang 15+ (macOS 14+)
 */
using source_location = std::source_location;
} // namespace mm

#define MM_SOURCE_LOCATION() (std::source_location::current())
