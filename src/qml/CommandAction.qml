// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

import QtQuick
import QtQuick.Controls as QQC2

// Binds a QQC2 Action's visible surface to a UiCommand (see
// ../mainwindow/UiCommand.h), so QML menus/toolbars can bind directly
// against CommandRegistry-owned commands (see ../mainwindow/CommandRegistry.h)
// instead of hand-wiring each Action's text/enabled/checkable/checked/
// shortcut and an onTriggered handler. Used by MainShell.qml (see
// shell/MainShell.qml) as `MenuItem { action: CommandAction { cmd: ... } }`.
//
// A null `cmd` -- an id with no registered command, or a command that
// exists but is registered disabled as a placeholder (see
// QmlShellWindow.cpp's ALL_COMMANDS/isLiveCommand()) -- degrades to a
// disabled, unlabeled, no-op action rather than erroring; only the
// disabled-placeholder case actually shows up in this commit's menus.
QQC2.Action {
    id: root

    // The UiCommand this action mirrors; see CommandRegistry::command().
    property var cmd: null

    // Mirrors UiCommand::toolTip -- the widget shell's QAction::statusTip()
    // text (see QmlShellWindow.cpp's CommandSpec::toolTip). Not a QQC2.Action
    // property, so it's exposed here for toolbar buttons to bind a hover
    // QQC2.ToolTip against (menus don't show tooltips natively).
    property string toolTip: cmd ? cmd.toolTip : ""

    text: cmd ? cmd.text : ""
    shortcut: cmd ? cmd.shortcut : ""
    enabled: cmd ? cmd.effectiveEnabled : false
    checkable: cmd ? cmd.checkable : false
    checked: cmd ? cmd.checked : false

    onTriggered: {
        if (cmd) {
            cmd.trigger();
        }
    }
}
