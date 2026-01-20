#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "../global/ChangeMonitor.h"
#include "../global/RuleOf5.h"
#include "../global/macros.h"
#include "HotkeyMacros.h"

class QSettings;

#include <array>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include <QString>
#include <Qt>

#define X_HOTKEY_MODS(base) \
    base, SHIFT_##base, CTRL_##base, CTRL_SHIFT_##base, ALT_##base, ALT_SHIFT_##base, \
        ALT_CTRL_##base, ALT_CTRL_SHIFT_##base, META_##base, META_SHIFT_##base, META_CTRL_##base, \
        META_CTRL_SHIFT_##base, META_ALT_##base, META_ALT_SHIFT_##base, META_ALT_CTRL_##base, \
        META_ALT_CTRL_SHIFT_##base

enum class HotkeyEnum : uint16_t {
#define X_ENUM(id, name, key, numpad) X_HOTKEY_MODS(id),
    XFOREACH_HOTKEY_BASE_KEYS(X_ENUM)
#undef X_ENUM
        COUNT,
    INVALID = 0xFFFF
};

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
};

class NODISCARD HotkeyManager final
{
private:
    // Unordered map for O(1) runtime lookups
    std::unordered_map<HotkeyEnum, std::string> m_hotkeys;

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
    NODISCARD std::vector<std::pair<QString, std::string>> getAllHotkeys() const;

    void resetToDefaults();
    void clear();

    NODISCARD static std::vector<QString> getAvailableKeyNames();
    NODISCARD static std::vector<QString> getAvailableModifiers();

    NODISCARD static HotkeyEnum qtKeyToHotkeyBase(int key, bool isNumpad);
    NODISCARD static QString hotkeyBaseToName(HotkeyEnum base);
    NODISCARD static HotkeyEnum nameToHotkeyBase(const QString &name);

private:
    ChangeMonitor::Lifetime m_configLifetime;
};
