// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

pragma Singleton
import QtQuick

// Shared touch hit-target sizing minimums for shell B (see the touch-target
// audit that motivated this file: many tap targets in the QML shell were
// 18-32px, well under the ~44px platform guideline). Deliberately a modest
// desktop-density compromise, not a full mobile redesign -- controlHeight
// stops at 40 (never exceeding the ~44px guideline) and rowHeight stays a
// touch smaller since list/table panels show many rows at once and a full
// jump to 40-44px per row would eat too much vertical space on desktop.
//
// Consumed via `import MMapper` + `Theme.<property>` everywhere the module
// is already imported (menus, toolbars, panel rows) -- see qmldir/
// qt_add_qml_module registration in ../CMakeLists.txt, which picks this up
// as a singleton purely from the `pragma Singleton` above.
QtObject {
    // Buttons, toolbar ToolButtons, and context-menu MenuItems.
    readonly property int controlHeight: 40

    // Compact list/table rows (GroupPanel/RoomPanel/TimerPanel).
    readonly property int rowHeight: 36
}
