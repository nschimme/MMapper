// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "Hotkey.h"

#include "../global/CaseUtils.h"
#include "../global/ConfigConsts-Computed.h"
#include "../global/TextUtils.h"

Hotkey::Hotkey(HotkeyEnum he)
    : m_hotkey(he)
{
    decompose();
}

Hotkey::Hotkey(HotkeyEnum base, uint8_t mods)
{
    if (base == HotkeyEnum::INVALID) {
        m_hotkey = HotkeyEnum::INVALID;
    } else {
        m_hotkey = static_cast<HotkeyEnum>(static_cast<uint16_t>(base) + (mods & AllModifiersMask));
    }
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

            XFOREACH_HOTKEY_MODIFIER(X_PARSE)
            {
                // This block is the final 'else' of the macro chain
                base = nameToHotkeyBase(part);
            }
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
        decompose();
        return;
    }

    uint8_t mods = Hotkey::qtModifiersToMask(modifiers);
    *this = Hotkey(base, mods);
}

void Hotkey::decompose()
{
    if (!isValid()) {
        m_policy = HotkeyPolicy::Any;
        return;
    }

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wswitch-enum"
#endif
    switch (base()) {
#define X_DECOMPOSE(id, name, qkey, policy) \
    case HotkeyEnum::id: \
        m_policy = policy; \
        break;
        XFOREACH_HOTKEY_BASE_KEYS(X_DECOMPOSE)
#undef X_DECOMPOSE
    default:
        m_policy = HotkeyPolicy::Any;
        break;
    }
#ifdef __clang__
#pragma clang diagnostic pop
#endif
}

std::string Hotkey::serialize() const
{
    if (!isValid()) {
        assert(false);
        return {};
    }

    const auto mods = modifiers();
    std::vector<std::string> parts;
#define X_STR(id, mod, bit) \
    if (mods & bit) \
        parts.emplace_back(#id);
    XFOREACH_HOTKEY_MODIFIER(X_STR)
#undef X_STR

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wswitch-enum"
#endif
    switch (base()) {
#define X_SERIALIZE(id, name, qkey, policy) \
    case HotkeyEnum::id: \
        parts.push_back(name); \
        break;
        XFOREACH_HOTKEY_BASE_KEYS(X_SERIALIZE)
#undef X_SERIALIZE
    default:
        assert(false);
        return {};
    }
#ifdef __clang__
#pragma clang diagnostic pop
#endif

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
    XFOREACH_HOTKEY_MODIFIER(X_MASK)
#undef X_MASK
    return mask;
}

HotkeyEnum Hotkey::qtKeyToHotkeyBase(int key, bool isNumpad)
{
    if constexpr (CURRENT_PLATFORM == PlatformEnum::Mac) {
        if (key == Qt::Key_Up || key == Qt::Key_Down || key == Qt::Key_Left
            || key == Qt::Key_Right) {
            isNumpad = false;
        }
    }
#define CHECK_NP(id, name, qk, pol) \
    if (isNumpad && pol == HotkeyPolicy::Keypad) \
        if (key == qk) \
            return HotkeyEnum::id;
    XFOREACH_HOTKEY_BASE_KEYS(CHECK_NP)
#undef CHECK_NP
    return HotkeyEnum::INVALID;
}

HotkeyEnum Hotkey::nameToHotkeyBase(std::string_view name)
{
    const std::string upperName = toUpperUtf8(name);

#define X_NAME_TO_ENUM(id, str, qkey, policy) \
    if (upperName == str) \
        return HotkeyEnum::id;
    XFOREACH_HOTKEY_BASE_KEYS(X_NAME_TO_ENUM)
#undef X_NAME_TO_ENUM
    return HotkeyEnum::INVALID;
}

std::vector<std::string> Hotkey::getAvailableKeyNames()
{
#define X_STR(id, name, key, policy) name,
    return {XFOREACH_HOTKEY_BASE_KEYS(X_STR)};
#undef X_STR
}

std::vector<std::string> Hotkey::getAvailableModifiers()
{
    return {
#define X_STR(id, mod, bit) #id,
        XFOREACH_HOTKEY_MODIFIER(X_STR)
#undef X_STR
    };
}
