#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#include "ChangeMonitor.h"
#include <string>

class INamedConfig
{
public:
    virtual ~INamedConfig() = default;
    virtual const std::string &getName() const = 0;
    virtual std::string toString() const = 0;
    virtual bool fromString(const std::string &str) = 0;
    virtual void registerChangeCallback(const ChangeMonitor::Lifetime &lifetime,
                                        ChangeMonitor::Function callback)
        = 0;
};
