// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "Hotkey.h"

#include "../global/CaseUtils.h"
#include "../global/ConfigConsts-Computed.h"
#include "../global/TextUtils.h"

Hotkey::Hotkey(HotkeyEnum base, HotkeyModifiers mods)
    : m_base(base)
    , m_modifiers(mods)
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
#define X_PARSE(id, mod) \
    if (isModifier(part, #id)) { \
        mods.insert(HotkeyModifierEnum::id); \
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

Hotkey::Hotkey(Qt::Key key, Qt::KeyboardModifiers modifiers)
{
    bool isNumpad = modifiers & Qt::KeypadModifier;
    m_base = Hotkey::qtKeyToHotkeyBase(key, isNumpad);
    if (m_base == HotkeyEnum::INVALID) {
        decompose();
        return;
    }

    m_modifiers = Hotkey::qtModifiersToHotkeyModifiers(modifiers);
    decompose();
}

void Hotkey::decompose()
{
    if (!isValid()) {
        m_policy = HotkeyPolicyEnum::Any;
        return;
    }

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wswitch-enum"
#endif
    switch (m_base) {
#define X_DECOMPOSE(id, name, qkey, policy) \
    case HotkeyEnum::id: \
        m_policy = policy; \
        break;
        XFOREACH_HOTKEY_BASE_KEYS(X_DECOMPOSE)
#undef X_DECOMPOSE
    default:
        m_policy = HotkeyPolicyEnum::Any;
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

    std::vector<std::string> parts;
    m_modifiers.for_each([&parts](HotkeyModifierEnum mod) {
        switch (mod) {
#define X_STR(id, mod) \
    case HotkeyModifierEnum::id: \
        parts.emplace_back(#id); \
        break;
            XFOREACH_HOTKEY_MODIFIER(X_STR)
#undef X_STR
        }
    });

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

HotkeyModifiers Hotkey::qtModifiersToHotkeyModifiers(Qt::KeyboardModifiers mods)
{
    HotkeyModifiers mask;
#define X_MASK(id, mod) \
    if (mods & mod) \
        mask.insert(HotkeyModifierEnum::id);
    XFOREACH_HOTKEY_MODIFIER(X_MASK)
#undef X_MASK
    return mask;
}

HotkeyEnum Hotkey::qtKeyToHotkeyBase(Qt::Key key, bool isNumpad)
{
    if constexpr (CURRENT_PLATFORM == PlatformEnum::Mac) {
        if (key == Qt::Key_Up || key == Qt::Key_Down || key == Qt::Key_Left
            || key == Qt::Key_Right) {
            isNumpad = false;
        }
    }

    if (isNumpad) {
        switch (key) {
        case Qt::Key_Up:
            return HotkeyEnum::NUMPAD8;
        case Qt::Key_Down:
            return HotkeyEnum::NUMPAD2;
        case Qt::Key_Left:
            return HotkeyEnum::NUMPAD4;
        case Qt::Key_Right:
            return HotkeyEnum::NUMPAD6;
        case Qt::Key_Home:
            return HotkeyEnum::NUMPAD7;
        case Qt::Key_End:
            return HotkeyEnum::NUMPAD1;
        case Qt::Key_PageUp:
            return HotkeyEnum::NUMPAD9;
        case Qt::Key_PageDown:
            return HotkeyEnum::NUMPAD3;
        case Qt::Key_Insert:
            return HotkeyEnum::NUMPAD0;
        case Qt::Key_Delete:
            return HotkeyEnum::NUMPAD_PERIOD;
        default:
            break;
        }
    }

#define X_CHECK_KEY(id, name, qk, pol) \
    if (key == qk && ((pol == HotkeyPolicyEnum::Keypad) == isNumpad)) \
        return HotkeyEnum::id;
    XFOREACH_HOTKEY_BASE_KEYS(X_CHECK_KEY)
#undef X_CHECK_KEY
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
#define X_STR(id, mod) #id,
        XFOREACH_HOTKEY_MODIFIER(X_STR)
#undef X_STR
    };
}
