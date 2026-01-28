// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "Hotkey.h"

#include "../global/CaseUtils.h"
#include "../global/TextUtils.h"

Hotkey::Hotkey(HotkeyEnum base, HotkeyModifiers mods)
    : m_base(base)
    , m_mods(mods)
{
    decompose();
}

Hotkey::Hotkey(HotkeyEnum base, uint8_t mods)
    : m_base(base)
    , m_mods(static_cast<HotkeyModifiers::underlying_type>(mods))
{
    decompose();
}

Hotkey::Hotkey(const QString &s)
    : Hotkey(mmqt::toStdStringUtf8(s))
{}

Hotkey::Hotkey(std::string_view s)
{
    if (s.empty()) {
        decompose();
        return;
    }

    HotkeyModifiers mods;
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
        mods.insert(HotkeyModifier::id); \
    } else
#define S_IGNORE(...)

            XFOREACH_HOTKEY_MODIFIER(X_PARSE, S_IGNORE)
            {
                // This block is the final 'else' of the macro chain
                base = nameToHotkeyBase(part);
            }
#undef S_IGNORE
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
    const bool isNumpad = (modifiers & Qt::KeypadModifier);
    const HotkeyEnum base = Hotkey::qtKeyToHotkeyBase(key, isNumpad);
    if (base == HotkeyEnum::INVALID) {
        decompose();
        return;
    }

    const HotkeyModifiers mods = Hotkey::qtModifiersToFlags(modifiers);
    *this = Hotkey(base, mods);
}

void Hotkey::decompose()
{
    if (!isValid()) {
        m_base = HotkeyEnum::INVALID;
        m_mods.clear();
        m_policy = HotkeyPolicy::Any;
        m_baseName = nullptr;
        return;
    }

    switch (m_base) {
#define X_DECOMPOSE(id, name, qkey, policy) \
    case HotkeyEnum::id: \
        m_policy = policy; \
        m_baseName = name; \
        break;
#define S_IGNORE(...)
        XFOREACH_HOTKEY_BASE_KEYS(X_DECOMPOSE, S_IGNORE)
#undef S_IGNORE
#undef X_DECOMPOSE
    default:
        m_policy = HotkeyPolicy::Any;
        m_baseName = nullptr;
        break;
    }
}

std::string Hotkey::serialize() const
{
    if (!isValid())
        return {};

    std::vector<std::string> parts;
#define X_STR(id, mod, bit) \
    if (m_mods.contains(HotkeyModifier::id)) \
        parts.emplace_back(#id);
#define S_IGNORE(...)
    XFOREACH_HOTKEY_MODIFIER(X_STR, S_IGNORE)
#undef S_IGNORE
#undef X_STR

    if (!m_baseName || *m_baseName == '\0')
        return {};

    parts.push_back(m_baseName);

    std::string result;
    for (size_t i = 0; i < parts.size(); ++i) {
        result += parts[i];
        if (i < parts.size() - 1) {
            result += "+";
        }
    }
    return result;
}

HotkeyModifiers Hotkey::qtModifiersToFlags(Qt::KeyboardModifiers mods)
{
    HotkeyModifiers flags;
#define X_MASK(id, mod, bit) \
    if (mods & mod) \
        flags.insert(HotkeyModifier::id);
#define S_IGNORE(...)
    XFOREACH_HOTKEY_MODIFIER(X_MASK, S_IGNORE)
#undef S_IGNORE
#undef X_MASK
    return flags;
}

HotkeyEnum Hotkey::qtKeyToHotkeyBase(int key, bool isNumpad)
{
    if (isNumpad) {
#define CHECK_NP(id, name, qk, pol) \
    if constexpr (pol == HotkeyPolicy::Keypad) \
        if (key == qk) \
            return HotkeyEnum::id;
#define S_CHECK_NP(id, qk, pol) \
    if constexpr (pol == HotkeyPolicy::Keypad) \
        if (key == qk) \
            return HotkeyEnum::id;
        XFOREACH_HOTKEY_BASE_KEYS(CHECK_NP, S_CHECK_NP)
#undef S_CHECK_NP
#undef CHECK_NP
    } else {
#define CHECK_OTH(id, name, qk, pol) \
    if constexpr (pol != HotkeyPolicy::Keypad) \
        if (key == qk) \
            return HotkeyEnum::id;
#define S_CHECK_OTH(id, qk, pol) \
    if constexpr (pol != HotkeyPolicy::Keypad) \
        if (key == qk) \
            return HotkeyEnum::id;
        XFOREACH_HOTKEY_BASE_KEYS(CHECK_OTH, S_CHECK_OTH)
#undef S_CHECK_OTH
#undef CHECK_OTH
    }

    return HotkeyEnum::INVALID;
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

const char *Hotkey::hotkeyBaseToName(HotkeyEnum base)
{
    switch (base) {
#define X_NAME_CHECK(id, name, qkey, policy) \
    case HotkeyEnum::id: \
        return name;
#define S_IGNORE(...)
        XFOREACH_HOTKEY_BASE_KEYS(X_NAME_CHECK, S_IGNORE)
#undef S_IGNORE
#undef X_NAME_CHECK
    default:
        return nullptr;
    }
}

HotkeyPolicy Hotkey::hotkeyBaseToPolicy(HotkeyEnum base)
{
    switch (base) {
#define X_POLICY_CHECK(id, name, qkey, policy) \
    case HotkeyEnum::id: \
        return policy;
#define S_IGNORE(...)
        XFOREACH_HOTKEY_BASE_KEYS(X_POLICY_CHECK, S_IGNORE)
#undef S_IGNORE
#undef X_POLICY_CHECK
    default:
        return HotkeyPolicy::Any;
    }
}
