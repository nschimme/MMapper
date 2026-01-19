#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "../global/RuleOf5.h"
#include "../global/macros.h"
#include "HotkeyMacros.h"

class QSettings;

#include <array>
#include <optional>
#include <string>
#include <vector>

#include <QString>
#include <Qt>

enum class HotkeyKeyEnum : uint8_t
{
#define X_ENUM(id, name, key, numpad) id,
    XFOREACH_HOTKEY_BASE_KEYS(X_ENUM)
#undef X_ENUM
        COUNT,
    INVALID = 255
};

class NODISCARD HotkeyCommand final
{
public:
    HotkeyKeyEnum baseKey = HotkeyKeyEnum::INVALID;
    uint8_t modifiers = 0; // 1: SHIFT, 2: CTRL, 4: ALT, 8: META

public:
    HotkeyCommand() = default;
    HotkeyCommand(HotkeyKeyEnum key, uint8_t mods)
        : baseKey(key)
        , modifiers(mods)
    {}

    NODISCARD bool isValid() const { return baseKey != HotkeyKeyEnum::INVALID; }

    NODISCARD QString serialize() const;
    NODISCARD static HotkeyCommand deserialize(const QString &s);

    NODISCARD bool operator==(const HotkeyCommand &other) const
    {
        return baseKey == other.baseKey && modifiers == other.modifiers;
    }

    NODISCARD static uint8_t qtModifiersToMask(Qt::KeyboardModifiers mods);
};

class NODISCARD HotkeyManager final
{
private:
    // O(1) runtime lookup table: [baseKey][modifierMask]
    std::array<std::string, static_cast<size_t>(HotkeyKeyEnum::COUNT) * 16> m_lookupTable;

public:
    HotkeyManager();
    ~HotkeyManager() = default;

    DELETE_CTORS_AND_ASSIGN_OPS(HotkeyManager);

    void read(const QSettings &settings);
    void write(QSettings &settings);

    NODISCARD bool setHotkey(const QString &keyName, const QString &command);
    NODISCARD bool setHotkey(const HotkeyCommand &hk, const std::string &command);
    void removeHotkey(const QString &keyName);

    NODISCARD std::optional<std::string> getCommand(int key,
                                                    Qt::KeyboardModifiers modifiers,
                                                    bool isNumpad) const;
    NODISCARD std::optional<std::string> getCommand(HotkeyKeyEnum key, uint8_t mask) const;
    NODISCARD std::optional<std::string> getCommand(const QString &keyName) const;
    NODISCARD std::optional<QString> getCommandQString(int key,
                                                       Qt::KeyboardModifiers modifiers,
                                                       bool isNumpad) const;
    NODISCARD std::optional<QString> getCommandQString(HotkeyKeyEnum key, uint8_t mask) const;
    NODISCARD std::optional<QString> getCommandQString(const QString &keyName) const;

    NODISCARD bool hasHotkey(const QString &keyName) const;
    NODISCARD std::vector<std::pair<QString, std::string>> getAllHotkeys() const;

    void resetToDefaults();
    void clear();

    NODISCARD static std::vector<QString> getAvailableKeyNames();
    NODISCARD static std::vector<QString> getAvailableModifiers();

    NODISCARD static HotkeyKeyEnum qtKeyToBaseKeyEnum(int key, bool isNumpad);

private:
    NODISCARD static QString baseKeyEnumToName(HotkeyKeyEnum key);
    NODISCARD static HotkeyKeyEnum nameToBaseKeyEnum(const QString &name);

    NODISCARD static size_t calculateIndex(HotkeyKeyEnum key, uint8_t mods)
    {
        return (static_cast<size_t>(key) << 4) | (mods & 0xF);
    }
};
