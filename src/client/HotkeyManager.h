#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "../global/ChangeMonitor.h"
#include "../global/RuleOf5.h"
#include "../global/macros.h"
#include "HotkeyMacros.h"

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include <QString>
#include <Qt>

class NODISCARD Hotkey final
{
private:
    HotkeyEnum m_hotkey = HotkeyEnum::INVALID;

public:
    Hotkey() = default;
    explicit Hotkey(HotkeyEnum he)
        : m_hotkey(he)
    {}
    Hotkey(HotkeyEnum base, uint8_t mods);

    NODISCARD bool isValid() const { return m_hotkey != HotkeyEnum::INVALID; }

    NODISCARD QString serialize() const;
    NODISCARD static Hotkey deserialize(const QString &s);

    NODISCARD bool operator==(const Hotkey &other) const { return m_hotkey == other.m_hotkey; }

    NODISCARD HotkeyEnum toEnum() const { return m_hotkey; }
    NODISCARD HotkeyEnum base() const;
    NODISCARD uint8_t modifiers() const;

    NODISCARD static uint8_t qtModifiersToMask(Qt::KeyboardModifiers mods);
    NODISCARD static HotkeyEnum qtKeyToHotkeyBase(int key, bool isNumpad);
    NODISCARD static QString hotkeyBaseToName(HotkeyEnum base);
    NODISCARD static HotkeyEnum nameToHotkeyBase(const QString &name);
    NODISCARD static std::vector<QString> getAvailableKeyNames();
    NODISCARD static std::vector<QString> getAvailableModifiers();
};

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
    NODISCARD bool setHotkey(const QString &keyName, const QString &command);
    NODISCARD bool setHotkey(const Hotkey &hk, const std::string &command);
    void removeHotkey(const QString &keyName);
    void removeHotkey(const Hotkey &hk);

    NODISCARD std::optional<std::string> getCommand(int key,
                                                    Qt::KeyboardModifiers modifiers,
                                                    bool isNumpad) const;
    NODISCARD std::optional<std::string> getCommand(const Hotkey &hk) const;
    NODISCARD std::optional<std::string> getCommand(const QString &keyName) const;
    NODISCARD std::optional<QString> getCommandQString(int key,
                                                       Qt::KeyboardModifiers modifiers,
                                                       bool isNumpad) const;
    NODISCARD std::optional<QString> getCommandQString(const Hotkey &hk) const;
    NODISCARD std::optional<QString> getCommandQString(const QString &keyName) const;

    NODISCARD bool hasHotkey(const QString &keyName) const;
    NODISCARD bool hasHotkey(const Hotkey &hk) const;
    NODISCARD std::vector<std::pair<Hotkey, std::string>> getAllHotkeys() const;

    void resetToDefaults();
    void clear();
};
