#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "../global/Flags.h"

#include <cstdint>
#include <string_view>

// X(UPPER_CASE, lower_case, CamelCase, "Friendly Name")
#define XFOREACH_SUB_AREA_FLAG(X) \
    X(MAZE, maze, Maze, "Maze") \
    X(TUNNEL, tunnel, Tunnel, "Tunnel") \
    X(HUB, hub, Hub, "Hub") \
    X(CHOKEPOINT, chokepoint, Chokepoint, "Chokepoint") \
    X(DISTANT, distant, Distant, "Distant") \
    X(LOCK_AUTO_CLASSIFY, lock_auto_classify, LockAutoClassify, "Lock Auto-Classify")

enum class NODISCARD SubAreaFlagEnum : uint8_t {
#define X_DECL_SUB_AREA_FLAG(UPPER_CASE, lower_case, CamelCase, friendly) UPPER_CASE,
    XFOREACH_SUB_AREA_FLAG(X_DECL_SUB_AREA_FLAG)
#undef X_DECL_SUB_AREA_FLAG
};

#define X_COUNT(UPPER_CASE, lower_case, CamelCase, friendly) +1
static constexpr const int NUM_SUB_AREA_FLAGS = XFOREACH_SUB_AREA_FLAG(X_COUNT);
#undef X_COUNT
DEFINE_ENUM_COUNT(SubAreaFlagEnum, NUM_SUB_AREA_FLAGS)

class NODISCARD SubAreaFlags final : public enums::Flags<SubAreaFlags, SubAreaFlagEnum, uint16_t>
{
public:
    using Flags::Flags;

public:
#define X_DEFINE_ACCESSORS(UPPER_CASE, lower_case, CamelCase, friendly) \
    NODISCARD bool is##CamelCase() const { return contains(SubAreaFlagEnum::UPPER_CASE); }
    XFOREACH_SUB_AREA_FLAG(X_DEFINE_ACCESSORS)
#undef X_DEFINE_ACCESSORS
};
DEFINE_FLAGS_BITOP_OR(SubAreaFlags)

NODISCARD inline std::string_view to_string_view(const SubAreaFlagEnum flag)
{
    switch (flag) {
#define X_CASE(UPPER_CASE, lower_case, CamelCase, friendly) \
    case SubAreaFlagEnum::UPPER_CASE: \
        return #lower_case;
        XFOREACH_SUB_AREA_FLAG(X_CASE)
#undef X_CASE
    }
    return "unknown";
}

NODISCARD inline std::string_view getName(const SubAreaFlagEnum flag)
{
    switch (flag) {
#define X_CASE(UPPER_CASE, lower_case, CamelCase, friendly) \
    case SubAreaFlagEnum::UPPER_CASE: \
        return friendly;
        XFOREACH_SUB_AREA_FLAG(X_CASE)
#undef X_CASE
    }
    return "Unknown";
}
