#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include <Qt>

// X-Macro for defining hotkeys. This allows us to define all hotkey information in one place.
// Format: X(name, qtKey, isNumpad)
// - name: The string name of the key (e.g., "F1", "NUMPAD8")
// - qtKey: The Qt::Key enum value
// - isNumpad: bool, true if this key is on the numpad and requires the KeypadModifier
#define X_FOREACH_HOTKEY(X) \
    X("F1", Qt::Key_F1, false) \
    X("F2", Qt::Key_F2, false) \
    X("F3", Qt::Key_F3, false) \
    X("F4", Qt::Key_F4, false) \
    X("F5", Qt::Key_F5, false) \
    X("F6", Qt::Key_F6, false) \
    X("F7", Qt::Key_F7, false) \
    X("F8", Qt::Key_F8, false) \
    X("F9", Qt::Key_F9, false) \
    X("F10", Qt::Key_F10, false) \
    X("F11", Qt::Key_F11, false) \
    X("F12", Qt::Key_F12, false) \
    X("NUMPAD0", Qt::Key_0, true) \
    X("NUMPAD1", Qt::Key_1, true) \
    X("NUMPAD2", Qt::Key_2, true) \
    X("NUMPAD3", Qt::Key_3, true) \
    X("NUMPAD4", Qt::Key_4, true) \
    X("NUMPAD5", Qt::Key_5, true) \
    X("NUMPAD6", Qt::Key_6, true) \
    X("NUMPAD7", Qt::Key_7, true) \
    X("NUMPAD8", Qt::Key_8, true) \
    X("NUMPAD9", Qt::Key_9, true) \
    X("NUMPAD_SLASH", Qt::Key_Slash, true) \
    X("NUMPAD_ASTERISK", Qt::Key_Asterisk, true) \
    X("NUMPAD_MINUS", Qt::Key_Minus, true) \
    X("NUMPAD_PLUS", Qt::Key_Plus, true) \
    X("NUMPAD_PERIOD", Qt::Key_Period, true) \
    X("HOME", Qt::Key_Home, false) \
    X("END", Qt::Key_End, false) \
    X("INSERT", Qt::Key_Insert, false) \
    X("PAGEUP", Qt::Key_PageUp, false) \
    X("PAGEDOWN", Qt::Key_PageDown, false) \
    X("UP", Qt::Key_Up, false) \
    X("DOWN", Qt::Key_Down, false) \
    X("LEFT", Qt::Key_Left, false) \
    X("RIGHT", Qt::Key_Right, false) \
    X("ACCENT", Qt::Key_QuoteLeft, false) \
    X("0", Qt::Key_0, false) \
    X("1", Qt::Key_1, false) \
    X("2", Qt::Key_2, false) \
    X("3", Qt::Key_3, false) \
    X("4", Qt::Key_4, false) \
    X("5", Qt::Key_5, false) \
    X("6", Qt::Key_6, false) \
    X("7", Qt::Key_7, false) \
    X("8", Qt::Key_8, false) \
    X("9", Qt::Key_9, false) \
    X("HYPHEN", Qt::Key_Minus, false) \
    X("EQUAL", Qt::Key_Equal, false)
