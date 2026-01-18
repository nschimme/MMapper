#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

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
    X("NUMPAD8", "n") \
    X("NUMPAD4", "w") \
    X("NUMPAD6", "e") \
    X("NUMPAD5", "s") \
    X("NUMPAD_MINUS", "u") \
    X("NUMPAD_PLUS", "d") \
    X("CTRL+NUMPAD8", "open exit n") \
    X("CTRL+NUMPAD4", "open exit w") \
    X("CTRL+NUMPAD6", "open exit e") \
    X("CTRL+NUMPAD5", "open exit s") \
    X("CTRL+NUMPAD_MINUS", "open exit u") \
    X("CTRL+NUMPAD_PLUS", "open exit d") \
    X("ALT+NUMPAD8", "close exit n") \
    X("ALT+NUMPAD4", "close exit w") \
    X("ALT+NUMPAD6", "close exit e") \
    X("ALT+NUMPAD5", "close exit s") \
    X("ALT+NUMPAD_MINUS", "close exit u") \
    X("ALT+NUMPAD_PLUS", "close exit d") \
    X("SHIFT+NUMPAD8", "pick exit n") \
    X("SHIFT+NUMPAD4", "pick exit w") \
    X("SHIFT+NUMPAD6", "pick exit e") \
    X("SHIFT+NUMPAD5", "pick exit s") \
    X("SHIFT+NUMPAD_MINUS", "pick exit u") \
    X("SHIFT+NUMPAD_PLUS", "pick exit d") \
    X("NUMPAD7", "look") \
    X("NUMPAD9", "flee") \
    X("NUMPAD2", "lead") \
    X("NUMPAD0", "bash") \
    X("NUMPAD1", "ride") \
    X("NUMPAD3", "stand")
