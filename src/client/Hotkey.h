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

// Macro to define all supported base keys
// X(EnumName, StringName, QtKey, IsNumpad)
#define XFOREACH_HOTKEY_BASE_KEYS(X) \
    X(F1, "F1", Qt::Key_F1, false) \
    X(F2, "F2", Qt::Key_F2, false) \
    X(F3, "F3", Qt::Key_F3, false) \
    X(F4, "F4", Qt::Key_F4, false) \
    X(F5, "F5", Qt::Key_F5, false) \
    X(F6, "F6", Qt::Key_F6, false) \
    X(F7, "F7", Qt::Key_F7, false) \
    X(F8, "F8", Qt::Key_F8, false) \
    X(F9, "F9", Qt::Key_F9, false) \
    X(F10, "F10", Qt::Key_F10, false) \
    X(F11, "F11", Qt::Key_F11, false) \
    X(F12, "F12", Qt::Key_F12, false) \
    X(NUMPAD0, "NUMPAD0", Qt::Key_0, true) \
    X(NUMPAD1, "NUMPAD1", Qt::Key_1, true) \
    X(NUMPAD2, "NUMPAD2", Qt::Key_2, true) \
    X(NUMPAD3, "NUMPAD3", Qt::Key_3, true) \
    X(NUMPAD4, "NUMPAD4", Qt::Key_4, true) \
    X(NUMPAD5, "NUMPAD5", Qt::Key_5, true) \
    X(NUMPAD6, "NUMPAD6", Qt::Key_6, true) \
    X(NUMPAD7, "NUMPAD7", Qt::Key_7, true) \
    X(NUMPAD8, "NUMPAD8", Qt::Key_8, true) \
    X(NUMPAD9, "NUMPAD9", Qt::Key_9, true) \
    X(NUMPAD_SLASH, "NUMPAD_SLASH", Qt::Key_Slash, true) \
    X(NUMPAD_ASTERISK, "NUMPAD_ASTERISK", Qt::Key_Asterisk, true) \
    X(NUMPAD_MINUS, "NUMPAD_MINUS", Qt::Key_Minus, true) \
    X(NUMPAD_PLUS, "NUMPAD_PLUS", Qt::Key_Plus, true) \
    X(NUMPAD_PERIOD, "NUMPAD_PERIOD", Qt::Key_Period, true) \
    X(HOME, "HOME", Qt::Key_Home, false) \
    X(END, "END", Qt::Key_End, false) \
    X(INSERT, "INSERT", Qt::Key_Insert, false) \
    X(PAGEUP, "PAGEUP", Qt::Key_PageUp, false) \
    X(PAGEDOWN, "PAGEDOWN", Qt::Key_PageDown, false) \
    X(UP, "UP", Qt::Key_Up, false) \
    X(DOWN, "DOWN", Qt::Key_Down, false) \
    X(LEFT, "LEFT", Qt::Key_Left, false) \
    X(RIGHT, "RIGHT", Qt::Key_Right, false) \
    X(ACCENT, "ACCENT", Qt::Key_QuoteLeft, false) \
    X(K_0, "0", Qt::Key_0, false) \
    X(K_1, "1", Qt::Key_1, false) \
    X(K_2, "2", Qt::Key_2, false) \
    X(K_3, "3", Qt::Key_3, false) \
    X(K_4, "4", Qt::Key_4, false) \
    X(K_5, "5", Qt::Key_5, false) \
    X(K_6, "6", Qt::Key_6, false) \
    X(K_7, "7", Qt::Key_7, false) \
    X(K_8, "8", Qt::Key_8, false) \
    X(K_9, "9", Qt::Key_9, false) \
    X(HYPHEN, "HYPHEN", Qt::Key_Minus, false) \
    X(EQUAL, "EQUAL", Qt::Key_Equal, false)

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

#define XFOREACH_HOTKEY_MODIFIER(X) \
    X(SHIFT, Qt::ShiftModifier, 1) \
    X(CTRL, Qt::ControlModifier, 2) \
    X(ALT, Qt::AltModifier, 4) \
    X(META, Qt::MetaModifier, 8)

#define PERMUTE_0(key) key,
#define PERMUTE_1(key) PERMUTE_0(key) PERMUTE_0(SHIFT_##key)
#define PERMUTE_2(key) PERMUTE_1(key) PERMUTE_1(CTRL_##key)
#define PERMUTE_3(key) PERMUTE_2(key) PERMUTE_2(ALT_##key)
#define PERMUTE_4(key) PERMUTE_3(key) PERMUTE_3(META_##key)
#define X_GENERATE_ALL_MODS(id, name, key, numpad) PERMUTE_4(id)

enum class HotkeyEnum : uint16_t { XFOREACH_HOTKEY_BASE_KEYS(X_GENERATE_ALL_MODS) INVALID };

#undef PERMUTE_0
#undef PERMUTE_1
#undef PERMUTE_2
#undef PERMUTE_3
#undef PERMUTE_4
#undef X_GENERATE_ALL_MODS

namespace {
#define X_COUNT_BASE(id, name, key, numpad) +1
static constexpr int NUM_BASES = 0 XFOREACH_HOTKEY_BASE_KEYS(X_COUNT_BASE);
#undef X_COUNT_BASE
#define X_COUNT_MODS(name, modifier, shift) +1
static constexpr int NUM_MOD_BITS = 0 XFOREACH_HOTKEY_MODIFIER(X_COUNT_MODS);
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
#define X_CHECK_UPPER(id, name, qkey, num) \
    static_assert(isUppercase(name), "Hotkey name must be uppercase: " name);
XFOREACH_HOTKEY_BASE_KEYS(X_CHECK_UPPER)
#undef X_CHECK_UPPER
} // namespace

class NODISCARD Hotkey final
{
private:
    HotkeyEnum m_hotkey = HotkeyEnum::INVALID;

public:
    Hotkey() = default;
    DEFAULT_RULE_OF_5(Hotkey);

    explicit Hotkey(HotkeyEnum he)
        : m_hotkey(he)
    {}
    Hotkey(HotkeyEnum base, uint8_t mods);
    explicit Hotkey(const QString &s);
    Hotkey(std::string_view s);
    Hotkey(const char *s)
        : Hotkey(std::string_view(s))
    {}
    Hotkey(int key, Qt::KeyboardModifiers modifiers, bool isNumpad);

    NODISCARD bool isValid() const { return m_hotkey != HotkeyEnum::INVALID; }

    NODISCARD std::string serialize() const;

    NODISCARD bool operator==(const Hotkey &other) const { return m_hotkey == other.m_hotkey; }

    NODISCARD HotkeyEnum toEnum() const { return m_hotkey; }
    NODISCARD HotkeyEnum base() const;
    NODISCARD uint8_t modifiers() const;

    NODISCARD static uint8_t qtModifiersToMask(Qt::KeyboardModifiers mods);
    NODISCARD static HotkeyEnum qtKeyToHotkeyBase(int key, bool isNumpad);
    NODISCARD static std::string hotkeyBaseToName(HotkeyEnum base);
    NODISCARD static HotkeyEnum nameToHotkeyBase(std::string_view name);
    NODISCARD static std::vector<std::string> getAvailableKeyNames();
    NODISCARD static std::vector<std::string> getAvailableModifiers();
};
