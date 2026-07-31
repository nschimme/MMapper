// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

import QtQuick

// Reusable on-screen-keyboard occlusion helper for QmlDialog-hosted
// dialogs, mirroring MainShell.qml's keyboardInset (see its "Scope 16"
// comment). Each QmlDialog is its own top-level window (see QmlDialog.h),
// so Qt.inputMethod.keyboardRectangle can be read directly here without any
// coordinate translation, exactly as MainShell reads it for its own window.
//
// Usage: instantiate once per dialog root, e.g.
//   KeyboardInset { id: keyboardInset }
// then bind a footer row's bottom margin (and/or a ScrollView's bottom
// anchor) to `keyboardInset.inset` so focused fields and action buttons
// stay above a soft keyboard instead of being covered by it.
//
// Pure no-op wherever the platform reports no input method -- desktop, and
// headless "offscreen" QPA in tests: Qt.inputMethod.visible is then false,
// so `inset` stays 0 and nothing moves. Headless offscreen has no input
// method to drive it, so the tests only assert that the binding compiles and
// evaluates to 0; the actual lift still needs confirming on a touch device.
QtObject {
    readonly property real inset: Qt.inputMethod.visible
        ? Qt.inputMethod.keyboardRectangle.height : 0
}
