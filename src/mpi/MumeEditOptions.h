#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "../global/macros.h"

#include <cstdint>
#include <optional>
#include <vector>

#include <QString>

struct NODISCARD MumeEditOptions final
{
public:
    struct Range final
    {
        char32_t start = 0;
        char32_t end = 0;
    };

public:
    std::optional<int> maxSize;
    std::vector<Range> ranges;
    bool allowAnsiSgr = false;
    bool allowEmoji = false;
    bool hasAllowedRanges = false;

public:
    NODISCARD static bool isEmojiCodepoint(const char32_t cp)
    {
        return (cp >= 0x1F600 && cp <= 0x1F64F) ||
               (cp >= 0x1F300 && cp <= 0x1F5FF) ||
               (cp >= 0x1F680 && cp <= 0x1F6FF) ||
               (cp >= 0x1F1E0 && cp <= 0x1F1FF) ||
               (cp >= 0x2600 && cp <= 0x26FF) ||
               (cp >= 0x2700 && cp <= 0x27BF) ||
               (cp >= 0x1F900 && cp <= 0x1F9FF) ||
               (cp >= 0x1FA70 && cp <= 0x1FAFF);
    }

    NODISCARD bool isCodepointAllowed(const char32_t cp) const
    {
        if (!hasAllowedRanges) {
            return true;
        }

        if (allowAnsiSgr && cp == 0x1B) {
            return true;
        }

        if (allowEmoji && isEmojiCodepoint(cp)) {
            return true;
        }

        for (const auto &r : ranges) {
            if (cp >= r.start && cp <= r.end) {
                return true;
            }
        }
        return false;
    }
};
