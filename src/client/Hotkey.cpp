// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "Hotkey.h"

#include "../global/CaseUtils.h"
#include "../global/TextUtils.h"

Hotkey::Hotkey(HotkeyEnum base, uint8_t mods)
{
    if (base == HotkeyEnum::INVALID) {
        m_hotkey = HotkeyEnum::INVALID;
    } else {
        m_hotkey = static_cast<HotkeyEnum>(static_cast<uint16_t>(base) + (mods & AllModifiersMask));
    }
}

Hotkey::Hotkey(const QString &s)
    : Hotkey(mmqt::toStdStringUtf8(s))
{}

Hotkey::Hotkey(std::string_view s)
{
    if (s.empty())
        return;

    uint8_t mods = 0;
    HotkeyEnum base = HotkeyEnum::INVALID;

    auto isModifier = [](std::string_view input, std::string_view target) {
        if (input.size() != target.size())
            return false;
        for (size_t i = 0; i < input.size(); ++i) {
            if (std::toupper(static_cast<unsigned char>(input[i]))
                != static_cast<unsigned char>(target[i])) {
                return false;
            }
        }
        return true;
    };

    size_t start = 0;
    size_t end = s.find('+');
    while (true) {
        std::string_view part = (end == std::string::npos) ? s.substr(start)
                                                           : s.substr(start, end - start);

        // Trim whitespace
        while (!part.empty() && std::isspace(static_cast<unsigned char>(part.front())))
            part.remove_prefix(1);
        while (!part.empty() && std::isspace(static_cast<unsigned char>(part.back())))
            part.remove_suffix(1);

        if (!part.empty()) {
// X-Macro expansion using the lambda
#define X_PARSE(id, mod, bit) \
    if (isModifier(part, #id)) { \
        mods |= bit; \
    } else
#define S_PARSE(id, str, mod, bit) \
    if (isModifier(part, str)) { \
        mods |= bit; \
    } else

            XFOREACH_HOTKEY_MODIFIER(X_PARSE, S_PARSE)
            {
                // This block is the final 'else' of the macro chain
                base = nameToHotkeyBase(part);
            }
#undef S_PARSE
#undef X_PARSE
        }

        if (end == std::string::npos)
            break;
        start = end + 1;
        end = s.find('+', start);
    }

    *this = Hotkey(base, mods);
}

Hotkey::Hotkey(int key, Qt::KeyboardModifiers modifiers)
{
    bool isNumpad = modifiers & Qt::KeypadModifier;
    HotkeyEnum base = Hotkey::qtKeyToHotkeyBase(key, isNumpad);
    if (base == HotkeyEnum::INVALID) {
        return;
    }

    uint8_t mods = Hotkey::qtModifiersToMask(modifiers);
    *this = Hotkey(base, mods);
}

HotkeyEnum Hotkey::base() const
{
    if (!isValid()) {
        return HotkeyEnum::INVALID;
    }
    return static_cast<HotkeyEnum>(static_cast<uint16_t>(m_hotkey) & ~AllModifiersMask);
}

uint8_t Hotkey::modifiers() const
{
    if (!isValid()) {
        return 0;
    }
    return static_cast<uint8_t>(static_cast<uint16_t>(m_hotkey) & AllModifiersMask);
}

HotkeyPolicy Hotkey::policy() const
{
    return hotkeyBaseToPolicy(base());
}

std::string Hotkey::serialize() const
{
    if (!isValid())
        return {};

    std::vector<std::string> parts;
    uint8_t mods = modifiers();
#define X_STR(id, mod, bit) \
    if (mods & bit) \
        parts.emplace_back(#id);
#define S_IGNORE(...)
    XFOREACH_HOTKEY_MODIFIER(X_STR, S_IGNORE)
#undef S_IGNORE
#undef X_STR

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
#define X_MASK(id, mod, bit) \
    if (mods & mod) \
        mask |= bit;
#define S_IGNORE(...)
    XFOREACH_HOTKEY_MODIFIER(X_MASK, S_IGNORE)
#undef S_IGNORE
#undef X_MASK
    return mask;
}

HotkeyEnum Hotkey::qtKeyToHotkeyBase(int key, bool isNumpad)
{
#define CHECK(id, qk, pol) \
    if (isNumpad == (pol == HotkeyPolicy::Keypad) && key == qk) \
        return HotkeyEnum::id;

#define X_QT_CHECK(id, name, qk, pol) CHECK(id, qk, pol)
#define S_QT_CHECK(id, qk, pol) CHECK(id, qk, pol)

    XFOREACH_HOTKEY_BASE_KEYS(X_QT_CHECK, S_QT_CHECK)

#undef S_QT_CHECK
#undef X_QT_CHECK
#undef CHECK

    return HotkeyEnum::INVALID;
}

std::string Hotkey::hotkeyBaseToName(HotkeyEnum base)
{
    switch (base) {
#define X_NAME_CHECK(id, name, qkey, policy) \
    case HotkeyEnum::id: return name;
#define S_IGNORE(...)
        XFOREACH_HOTKEY_BASE_KEYS(X_NAME_CHECK, S_IGNORE)
#undef S_IGNORE
#undef X_NAME_CHECK
    default: return {};
    }
}

HotkeyPolicy Hotkey::hotkeyBaseToPolicy(HotkeyEnum base)
{
    switch (base) {
#define X_POLICY_CHECK(id, name, qkey, policy) \
    case HotkeyEnum::id: return policy;
#define S_IGNORE(...)
        XFOREACH_HOTKEY_BASE_KEYS(X_POLICY_CHECK, S_IGNORE)
#undef S_IGNORE
#undef X_POLICY_CHECK
    default: return HotkeyPolicy::Any;
    }
}

HotkeyEnum Hotkey::nameToHotkeyBase(std::string_view name)
{
    const std::string upperName = toUpperUtf8(name);

#define X_NAME_TO_ENUM(id, str, qkey, policy) \
    if (upperName == str) \
        return HotkeyEnum::id;
#define S_IGNORE(...)
    XFOREACH_HOTKEY_BASE_KEYS(X_NAME_TO_ENUM, S_IGNORE)
#undef S_IGNORE
#undef X_NAME_TO_ENUM
    return HotkeyEnum::INVALID;
}

std::vector<std::string> Hotkey::getAvailableKeyNames()
{
#define X_STR(id, name, key, policy) name,
#define S_IGNORE(...)
    return {XFOREACH_HOTKEY_BASE_KEYS(X_STR, S_IGNORE)};
#undef S_IGNORE
#undef X_STR
}

std::vector<std::string> Hotkey::getAvailableModifiers()
{
    return {
#define X_STR(id, mod, bit) #id,
#define S_IGNORE(...)
        XFOREACH_HOTKEY_MODIFIER(X_STR, S_IGNORE)
#undef S_IGNORE
#undef X_STR
    };
}
