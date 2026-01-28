#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "../global/RuleOf5.h"
#include "../global/macros.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include <QString>
#include <Qt>

enum class HotkeyPolicy : uint8_t {
    Any,              // Can be bound with or without modifiers (e.g. F-keys)
    Keypad,           // Can be bound with or without modifiers (e.g. Numpad)
    ModifierRequired, // Requires any modifier (CTRL, ALT, or SHIFT) to be bound (e.g. Arrows)
    ModifierNotShift, // Requires a non-SHIFT modifier (CTRL or ALT) (e.g. 1, -, =)
};

// Macro to define all supported base keys and their Qt mappings.
//
// X(EnumName, StringName, QtKey, Policy) -> Defines a unique identity
// S(EnumName, QtKey, Policy)             -> Defines an additional mapping (alias)
//
// Secondary mappings (aliases) are needed because Qt often reports navigation keys
// (e.g. Qt::Key_Left instead of Qt::Key_4) for Numpad keys when Shift is held or
// NumLock is off, but it still includes the Qt::KeypadModifier. Aliases allow
// these events to be correctly mapped back to the NUMPAD hotkey identity.
#define XFOREACH_HOTKEY_BASE_KEYS(X, S) \
    X(F1, "F1", Qt::Key_F1, HotkeyPolicy::Any) \
    X(F2, "F2", Qt::Key_F2, HotkeyPolicy::Any) \
    X(F3, "F3", Qt::Key_F3, HotkeyPolicy::Any) \
    X(F4, "F4", Qt::Key_F4, HotkeyPolicy::Any) \
    X(F5, "F5", Qt::Key_F5, HotkeyPolicy::Any) \
    X(F6, "F6", Qt::Key_F6, HotkeyPolicy::Any) \
    X(F7, "F7", Qt::Key_F7, HotkeyPolicy::Any) \
    X(F8, "F8", Qt::Key_F8, HotkeyPolicy::Any) \
    X(F9, "F9", Qt::Key_F9, HotkeyPolicy::Any) \
    X(F10, "F10", Qt::Key_F10, HotkeyPolicy::Any) \
    X(F11, "F11", Qt::Key_F11, HotkeyPolicy::Any) \
    X(F12, "F12", Qt::Key_F12, HotkeyPolicy::Any) \
    X(NUMPAD0, "NUMPAD0", Qt::Key_0, HotkeyPolicy::Keypad) \
    S(NUMPAD0, Qt::Key_Insert, HotkeyPolicy::Keypad) \
    X(NUMPAD1, "NUMPAD1", Qt::Key_1, HotkeyPolicy::Keypad) \
    S(NUMPAD1, Qt::Key_End, HotkeyPolicy::Keypad) \
    X(NUMPAD2, "NUMPAD2", Qt::Key_2, HotkeyPolicy::Keypad) \
    S(NUMPAD2, Qt::Key_Down, HotkeyPolicy::Keypad) \
    X(NUMPAD3, "NUMPAD3", Qt::Key_3, HotkeyPolicy::Keypad) \
    S(NUMPAD3, Qt::Key_PageDown, HotkeyPolicy::Keypad) \
    X(NUMPAD4, "NUMPAD4", Qt::Key_4, HotkeyPolicy::Keypad) \
    S(NUMPAD4, Qt::Key_Left, HotkeyPolicy::Keypad) \
    X(NUMPAD5, "NUMPAD5", Qt::Key_5, HotkeyPolicy::Keypad) \
    S(NUMPAD5, Qt::Key_Clear, HotkeyPolicy::Keypad) \
    X(NUMPAD6, "NUMPAD6", Qt::Key_6, HotkeyPolicy::Keypad) \
    S(NUMPAD6, Qt::Key_Right, HotkeyPolicy::Keypad) \
    X(NUMPAD7, "NUMPAD7", Qt::Key_7, HotkeyPolicy::Keypad) \
    S(NUMPAD7, Qt::Key_Home, HotkeyPolicy::Keypad) \
    X(NUMPAD8, "NUMPAD8", Qt::Key_8, HotkeyPolicy::Keypad) \
    S(NUMPAD8, Qt::Key_Up, HotkeyPolicy::Keypad) \
    X(NUMPAD9, "NUMPAD9", Qt::Key_9, HotkeyPolicy::Keypad) \
    S(NUMPAD9, Qt::Key_PageUp, HotkeyPolicy::Keypad) \
    X(NUMPAD_SLASH, "NUMPAD_SLASH", Qt::Key_Slash, HotkeyPolicy::Keypad) \
    X(NUMPAD_ASTERISK, "NUMPAD_ASTERISK", Qt::Key_Asterisk, HotkeyPolicy::Keypad) \
    X(NUMPAD_MINUS, "NUMPAD_MINUS", Qt::Key_Minus, HotkeyPolicy::Keypad) \
    X(NUMPAD_PLUS, "NUMPAD_PLUS", Qt::Key_Plus, HotkeyPolicy::Keypad) \
    X(NUMPAD_PERIOD, "NUMPAD_PERIOD", Qt::Key_Period, HotkeyPolicy::Keypad) \
    S(NUMPAD_PERIOD, Qt::Key_Delete, HotkeyPolicy::Keypad) \
    X(HOME, "HOME", Qt::Key_Home, HotkeyPolicy::ModifierRequired) \
    X(END, "END", Qt::Key_End, HotkeyPolicy::ModifierRequired) \
    X(INSERT, "INSERT", Qt::Key_Insert, HotkeyPolicy::ModifierRequired) \
    X(PAGEUP, "PAGEUP", Qt::Key_PageUp, HotkeyPolicy::ModifierRequired) \
    X(PAGEDOWN, "PAGEDOWN", Qt::Key_PageDown, HotkeyPolicy::ModifierRequired) \
    X(UP, "UP", Qt::Key_Up, HotkeyPolicy::ModifierRequired) \
    X(DOWN, "DOWN", Qt::Key_Down, HotkeyPolicy::ModifierRequired) \
    X(LEFT, "LEFT", Qt::Key_Left, HotkeyPolicy::ModifierRequired) \
    X(RIGHT, "RIGHT", Qt::Key_Right, HotkeyPolicy::ModifierRequired) \
    X(ACCENT, "ACCENT", Qt::Key_QuoteLeft, HotkeyPolicy::ModifierNotShift) \
    X(K_0, "0", Qt::Key_0, HotkeyPolicy::ModifierNotShift) \
    X(K_1, "1", Qt::Key_1, HotkeyPolicy::ModifierNotShift) \
    X(K_2, "2", Qt::Key_2, HotkeyPolicy::ModifierNotShift) \
    X(K_3, "3", Qt::Key_3, HotkeyPolicy::ModifierNotShift) \
    X(K_4, "4", Qt::Key_4, HotkeyPolicy::ModifierNotShift) \
    X(K_5, "5", Qt::Key_5, HotkeyPolicy::ModifierNotShift) \
    X(K_6, "6", Qt::Key_6, HotkeyPolicy::ModifierNotShift) \
    X(K_7, "7", Qt::Key_7, HotkeyPolicy::ModifierNotShift) \
    X(K_8, "8", Qt::Key_8, HotkeyPolicy::ModifierNotShift) \
    X(K_9, "9", Qt::Key_9, HotkeyPolicy::ModifierNotShift) \
    X(HYPHEN, "HYPHEN", Qt::Key_Minus, HotkeyPolicy::ModifierNotShift) \
    X(EQUAL, "EQUAL", Qt::Key_Equal, HotkeyPolicy::ModifierNotShift)

// Macro to define default hotkeys
// X(SerializedKey, Command)
#define XFOREACH_DEFAULT_HOTKEYS(X) \
    X("F1", "F1") \
    X("F2", "F2") \
    X("F3", "F3") \
    X("F4", "F4") \
    X("F5", "F5") \
    X("F6", "F6") \
    X("F7", "F7") \
    X("F8", "F8") \
    X("F9", "F9") \
    X("F10", "F10") \
    X("F11", "F11") \
    X("F12", "F12") \
    X("NUMPAD8", "north") \
    X("NUMPAD4", "west") \
    X("NUMPAD6", "east") \
    X("NUMPAD5", "south") \
    X("NUMPAD_MINUS", "up") \
    X("NUMPAD_PLUS", "down") \
    X("CTRL+NUMPAD8", "open exit north") \
    X("CTRL+NUMPAD4", "open exit west") \
    X("CTRL+NUMPAD6", "open exit east") \
    X("CTRL+NUMPAD5", "open exit south") \
    X("CTRL+NUMPAD_MINUS", "open exit up") \
    X("CTRL+NUMPAD_PLUS", "open exit dd") \
    X("ALT+NUMPAD8", "close exit north") \
    X("ALT+NUMPAD4", "close exit west") \
    X("ALT+NUMPAD6", "close exit east") \
    X("ALT+NUMPAD5", "close exit south") \
    X("ALT+NUMPAD_MINUS", "close exit up") \
    X("ALT+NUMPAD_PLUS", "close exit down") \
    X("SHIFT+NUMPAD8", "pick exit north") \
    X("SHIFT+NUMPAD4", "pick exit west") \
    X("SHIFT+NUMPAD6", "pick exit east") \
    X("SHIFT+NUMPAD5", "pick exit south") \
    X("SHIFT+NUMPAD_MINUS", "pick exit up") \
    X("SHIFT+NUMPAD_PLUS", "pick exit down") \
    X("NUMPAD7", "look") \
    X("NUMPAD9", "flee") \
    X("NUMPAD2", "lead") \
    X("NUMPAD0", "bash") \
    X("NUMPAD1", "ride") \
    X("NUMPAD3", "stand")

#define XFOREACH_HOTKEY_MODIFIER(X, S) \
    X(SHIFT, Qt::ShiftModifier, 1) \
    X(CTRL, Qt::ControlModifier, 2) \
    X(ALT, Qt::AltModifier, 4) \
    X(META, Qt::MetaModifier, 8)

#define PERMUTE_0(key) key,
#define PERMUTE_1(key) PERMUTE_0(key) PERMUTE_0(SHIFT_##key)
#define PERMUTE_2(key) PERMUTE_1(key) PERMUTE_1(CTRL_##key)
#define PERMUTE_3(key) PERMUTE_2(key) PERMUTE_2(ALT_##key)
#define PERMUTE_4(key) PERMUTE_3(key) PERMUTE_3(META_##key)
#define X_GENERATE_ALL_MODS(id, name, key, policy) PERMUTE_4(id)
#define S_IGNORE(...)

enum class HotkeyEnum : uint16_t {
    XFOREACH_HOTKEY_BASE_KEYS(X_GENERATE_ALL_MODS, S_IGNORE) INVALID
};

#undef PERMUTE_0
#undef PERMUTE_1
#undef PERMUTE_2
#undef PERMUTE_3
#undef PERMUTE_4
#undef X_GENERATE_ALL_MODS
#undef S_IGNORE

namespace {
#define X_COUNT_BASE(id, name, key, policy) +1
#define S_IGNORE(...)
static constexpr int NUM_BASES = 0 XFOREACH_HOTKEY_BASE_KEYS(X_COUNT_BASE, S_IGNORE);
#undef X_COUNT_BASE
#undef S_IGNORE
#define X_COUNT_MODS(id, modifier, bit) +1
#define S_IGNORE(...)
static constexpr int NUM_MOD_BITS = 0 XFOREACH_HOTKEY_MODIFIER(X_COUNT_MODS, S_IGNORE);
#undef S_IGNORE
#undef X_COUNT_MODS
static constexpr int VARIANTS_PER_KEY = 1 << NUM_MOD_BITS;
static constexpr int TOTAL_EXPECTED = NUM_BASES * VARIANTS_PER_KEY;
static_assert(TOTAL_EXPECTED == 784, "Total keys count changed");
static_assert(static_cast<int>(HotkeyEnum::INVALID) == TOTAL_EXPECTED, "Enum mismatch");

constexpr bool isUppercase(const char *s)
{
    while (*s) {
        if (*s >= 'a' && *s <= 'z')
            return false;
        s++;
    }
    return true;
}
#define X_CHECK_UPPER(id, name, qkey, policy) \
    static_assert(isUppercase(name), "Hotkey name must be uppercase: " name);
#define S_IGNORE(...)
XFOREACH_HOTKEY_BASE_KEYS(X_CHECK_UPPER, S_IGNORE)
#undef X_CHECK_UPPER
#undef S_IGNORE
} // namespace

class NODISCARD Hotkey final
{
public:
#define X_MOD_MASK(id, mod, bit) static constexpr uint8_t id##_MASK = bit;
#define S_IGNORE(...)
    XFOREACH_HOTKEY_MODIFIER(X_MOD_MASK, S_IGNORE)
#undef S_IGNORE
#undef X_MOD_MASK

#define X_MOD_TOTAL(id, mod, bit) | bit
#define S_IGNORE(...)
    static constexpr uint8_t AllModifiersMask = 0 XFOREACH_HOTKEY_MODIFIER(X_MOD_TOTAL, S_IGNORE);
#undef S_IGNORE
#undef X_MOD_TOTAL

private:
    HotkeyEnum m_hotkey = HotkeyEnum::INVALID;
    HotkeyEnum m_base = HotkeyEnum::INVALID;
    uint8_t m_mods = 0;
    HotkeyPolicy m_policy = HotkeyPolicy::Any;
    const char *m_baseName = nullptr;

public:
    Hotkey() = default;
    DEFAULT_RULE_OF_5(Hotkey);

    explicit Hotkey(HotkeyEnum he);
    Hotkey(HotkeyEnum base, uint8_t mods);
    explicit Hotkey(const QString &s);
    Hotkey(std::string_view s);
    Hotkey(const char *s)
        : Hotkey(std::string_view(s))
    {}
    Hotkey(int key, Qt::KeyboardModifiers modifiers);

    NODISCARD bool isValid() const { return m_hotkey != HotkeyEnum::INVALID; }

    NODISCARD std::string serialize() const;

    NODISCARD bool operator==(const Hotkey &other) const { return m_hotkey == other.m_hotkey; }

    NODISCARD HotkeyEnum toEnum() const { return m_hotkey; }
    NODISCARD HotkeyEnum base() const { return m_base; }
    NODISCARD uint8_t modifiers() const { return m_mods; }
    NODISCARD HotkeyPolicy policy() const { return m_policy; }

    NODISCARD static uint8_t qtModifiersToMask(Qt::KeyboardModifiers mods);
    NODISCARD static HotkeyEnum qtKeyToHotkeyBase(int key, bool isNumpad);
    NODISCARD static const char *hotkeyBaseToName(HotkeyEnum base);
    NODISCARD static HotkeyPolicy hotkeyBaseToPolicy(HotkeyEnum base);
    NODISCARD static HotkeyEnum nameToHotkeyBase(std::string_view name);
    NODISCARD static std::vector<std::string> getAvailableKeyNames();
    NODISCARD static std::vector<std::string> getAvailableModifiers();

private:
    void decompose();
};
