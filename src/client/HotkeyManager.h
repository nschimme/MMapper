#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "../global/ChangeMonitor.h"
#include "../global/RuleOf5.h"
#include "../global/macros.h"
#include "Hotkey.h"

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include <QString>
#include <Qt>

class NODISCARD HotkeyManager final
{
private:
    std::unordered_map<HotkeyEnum, std::string> m_hotkeys;
    ChangeMonitor::Lifetime m_configLifetime;

public:
    HotkeyManager();
    ~HotkeyManager();

    DELETE_CTORS_AND_ASSIGN_OPS(HotkeyManager);

private:
    void syncFromConfig();

public:
    NODISCARD bool setHotkey(const Hotkey &hk, std::string command);
    void removeHotkey(const Hotkey &hk);

    NODISCARD std::optional<std::string> getCommand(const Hotkey &hk) const;

    NODISCARD bool hasHotkey(const Hotkey &hk) const;
    NODISCARD std::vector<std::pair<Hotkey, std::string>> getAllHotkeys() const;

    void resetToDefaults();
    void clear();
};
