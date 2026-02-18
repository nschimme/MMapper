#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "../global/RuleOf5.h"
#include "legacy/Legacy.h"

#include <functional>
#include <unordered_map>

namespace Legacy {
class Functions;
}

class UboManager final
{
public:
    using UpdateFn = std::function<void(Legacy::Functions &)>;

    UboManager() = default;
    DELETE_CTORS_AND_ASSIGN_OPS(UboManager);

public:
    void registerUbo(Legacy::SharedVboEnum block, UpdateFn updateFn);
    void invalidate(Legacy::SharedVboEnum block);
    void updateAndBind(Legacy::Functions &gl, Legacy::SharedVboEnum block);

private:
    struct Entry
    {
        UpdateFn updateFn;
        bool dirty = true;
    };
    std::unordered_map<Legacy::SharedVboEnum, Entry> m_ubos;
};
