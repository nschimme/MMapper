// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "Hotkey.h"

#include "../global/CaseUtils.h"
#include "../global/TextUtils.h"

#include <algorithm>
#include <sstream>

Hotkey::Hotkey(HotkeyEnum base, uint8_t mods)
{
    if (base == HotkeyEnum::INVALID) {
        m_hotkey = HotkeyEnum::INVALID;
    } else {
        m_hotkey = static_cast<HotkeyEnum>(static_cast<uint16_t>(base) + (mods & 0xF));
    }
}

Hotkey::Hotkey(const QString &s)
    : Hotkey(mmqt::toStdStringUtf8(s))
{}

Hotkey::Hotkey(std::string_view s)
{
    if (s.empty()) {
        return;
    }

    uint8_t mods = 0;
    HotkeyEnum base = HotkeyEnum::INVALID;

    const std::string str = toUpperUtf8(s);

    size_t start = 0;
    size_t end = str.find('+');
    while (true) {
        std::string_view part;
        if (end == std::string::npos) {
            part = std::string_view(str).substr(start);
        } else {
            part = std::string_view(str).substr(start, end - start);
        }

        // Trim
        while (!part.empty() && std::isspace(static_cast<unsigned char>(part.front())))
            part.remove_prefix(1);
        while (!part.empty() && std::isspace(static_cast<unsigned char>(part.back())))
            part.remove_suffix(1);

        if (part == "SHIFT") {
            mods |= 1;
        } else if (part == "CTRL" || part == "CONTROL") {
            mods |= 2;
        } else if (part == "ALT") {
            mods |= 4;
        } else if (part == "META" || part == "CMD" || part == "COMMAND") {
            mods |= 8;
        } else if (!part.empty()) {
            base = nameToHotkeyBase(part);
        }

        if (end == std::string::npos) {
            break;
        }
        start = end + 1;
        end = str.find('+', start);
    }

    *this = Hotkey(base, mods);
}

Hotkey::Hotkey(int key, Qt::KeyboardModifiers modifiers, bool isNumpad)
{
    HotkeyEnum base = Hotkey::qtKeyToHotkeyBase(key, isNumpad);
    if (base == HotkeyEnum::INVALID) {
        return;
    }

    uint8_t mods = Hotkey::qtModifiersToMask(modifiers);
    *this = Hotkey(base, mods);
}

HotkeyEnum Hotkey::base() const
{
    if (!isValid())
        return HotkeyEnum::INVALID;
    return static_cast<HotkeyEnum>(static_cast<uint16_t>(m_hotkey) & 0xFFF0);
}

uint8_t Hotkey::modifiers() const
{
    if (!isValid())
        return 0;
    return static_cast<uint8_t>(static_cast<uint16_t>(m_hotkey) & 0xF);
}

std::string Hotkey::serialize() const
{
    if (!isValid())
        return {};

    std::vector<std::string> parts;
    uint8_t mods = modifiers();
    if (mods & 1)
        parts.emplace_back("SHIFT");
    if (mods & 2)
        parts.emplace_back("CTRL");
    if (mods & 4)
        parts.emplace_back("ALT");
    if (mods & 8)
        parts.emplace_back("META");

    std::string name = hotkeyBaseToName(base());
    if (name.empty())
        return {};

    parts.push_back(std::move(name));

    std::string result;
    for (size_t i = 0; i < parts.size(); ++i) {
        result += parts[i];
        if (i < parts.size() - 1) {
            result += "+";
        }
    }
    return result;
}

uint8_t Hotkey::qtModifiersToMask(Qt::KeyboardModifiers mods)
{
    uint8_t mask = 0;
    if (mods & Qt::ShiftModifier)
        mask |= 1;
    if (mods & Qt::ControlModifier)
        mask |= 2;
    if (mods & Qt::AltModifier)
        mask |= 4;
    if (mods & Qt::MetaModifier)
        mask |= 8;
    return mask;
}

HotkeyEnum Hotkey::qtKeyToHotkeyBase(int key, bool isNumpad)
{
#define X_QT_CHECK(id, name, qkey, num) \
    if (key == qkey && isNumpad == num) \
        return HotkeyEnum::id;
    XFOREACH_HOTKEY_BASE_KEYS(X_QT_CHECK)
#undef X_QT_CHECK
    return HotkeyEnum::INVALID;
}

std::string Hotkey::hotkeyBaseToName(HotkeyEnum base)
{
#define X_NAME_CHECK(id, name, qkey, num) \
    if (base == HotkeyEnum::id) \
        return name;
    XFOREACH_HOTKEY_BASE_KEYS(X_NAME_CHECK)
#undef X_NAME_CHECK
    return {};
}

HotkeyEnum Hotkey::nameToHotkeyBase(std::string_view name)
{
    const std::string upperName = toUpperUtf8(name);

#define X_NAME_TO_ENUM(id, str, qkey, num) \
    if (upperName == str) \
        return HotkeyEnum::id;
    XFOREACH_HOTKEY_BASE_KEYS(X_NAME_TO_ENUM)
#undef X_NAME_TO_ENUM
    return HotkeyEnum::INVALID;
}

std::vector<std::string> Hotkey::getAvailableKeyNames()
{
    return {
#define X_STR(id, name, key, numpad) name,
        XFOREACH_HOTKEY_BASE_KEYS(X_STR)
#undef X_STR
    };
}

std::vector<std::string> Hotkey::getAvailableModifiers()
{
    return {
#define X_STR(name) #name,
        XFOREACH_HOTKEY_MODIFIER(X_STR)
#undef X_STR
    };
}
