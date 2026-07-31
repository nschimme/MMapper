// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

import QtQuick
import QtQuick.Controls

import MMapper

// Integrated Client preferences page, driven by preferencesController.client
// (a ClientPageAdapter, see ../preferences/ClientPageAdapter.h). Mirrors
// clientpage.ui's Font/Color/Terminal group boxes without a .ui file. The
// font/color pickers stay native (QFontDialog/QColorDialog, launched via the
// adapter's chooseFont()/chooseForegroundColor()/chooseBackgroundColor()
// invokables).
Column {
    id: root
    spacing: 8

    readonly property var client: preferencesController.client

    Label { text: qsTr("Font and Colors"); font.bold: true }

    Flow {
        width: parent.width
        spacing: 8
        Label { text: qsTr("Font:"); width: 120 }
        Button {
            text: root.client.fontDisplayName
            onClicked: root.client.chooseFont()
        }
    }

    Flow {
        width: parent.width
        spacing: 8
        Label { text: qsTr("Foreground:"); width: 120 }
        // Flow positions its direct children itself and rejects anchors on
        // them, so the swatch is centered against the taller Select button
        // via this full-height spacer instead (anchors on a grandchild are
        // fine). Without it the 16px swatch top-aligns against the ~40px
        // button.
        Item {
            width: 24
            height: Theme.controlHeight
            Rectangle {
                anchors.centerIn: parent
                width: 24
                height: 16
                border.color: "black"
                border.width: 1
                color: root.client.foregroundColor
            }
        }
        Button {
            text: qsTr("Select")
            onClicked: root.client.chooseForegroundColor()
        }
    }

    Flow {
        width: parent.width
        spacing: 8
        Label { text: qsTr("Background:"); width: 120 }
        // Full-height spacer so the swatch centers against the Select
        // button -- see the Foreground row above.
        Item {
            width: 24
            height: Theme.controlHeight
            Rectangle {
                anchors.centerIn: parent
                width: 24
                height: 16
                border.color: "black"
                border.width: 1
                color: root.client.backgroundColor
            }
        }
        Button {
            text: qsTr("Select")
            onClicked: root.client.chooseBackgroundColor()
        }
    }

    Label {
        text: qsTr("Example Text")
        color: root.client.foregroundColor
        padding: 4
        background: Rectangle { color: root.client.backgroundColor }
    }

    Label { text: qsTr("Terminal"); font.bold: true }

    Row {
        spacing: 8
        Label { text: qsTr("Columns:"); width: 160 }
        SpinBox {
            // Mirrors clientpage.ui's columnsSpinBox range.
            editable: true
            from: 80
            to: 1000
            value: root.client.columns
            onValueModified: root.client.columns = value
        }
    }
    Row {
        spacing: 8
        Label { text: qsTr("Rows:"); width: 160 }
        SpinBox {
            // Mirrors clientpage.ui's rowsSpinBox range.
            editable: true
            from: 24
            to: 1000
            value: root.client.rows
            onValueModified: root.client.rows = value
        }
    }
    Row {
        spacing: 8
        Label { text: qsTr("Lines of scrollback:"); width: 160 }
        SpinBox {
            // Mirrors clientpage.ui's scrollbackSpinBox range.
            editable: true
            from: 0
            to: 2147483647
            value: root.client.linesOfScrollback
            onValueModified: root.client.linesOfScrollback = value
        }
    }
    Row {
        spacing: 8
        Label { text: qsTr("Lines of peek preview:"); width: 160 }
        SpinBox {
            // clientpage.ui's previewSpinBox leaves QSpinBox's default
            // range (0-99) unset.
            editable: true
            from: 0
            to: 99
            value: root.client.linesOfPeekPreview
            onValueModified: root.client.linesOfPeekPreview = value
        }
    }
    Row {
        spacing: 8
        Label { text: qsTr("Lines of input history:"); width: 160 }
        SpinBox {
            // Mirrors clientpage.ui's inputHistorySpinBox range.
            editable: true
            from: 0
            to: 1000
            value: root.client.linesOfInputHistory
            onValueModified: root.client.linesOfInputHistory = value
        }
    }
    Row {
        spacing: 8
        Label { text: qsTr("Tab dictionary size:"); width: 160 }
        SpinBox {
            // Mirrors clientpage.ui's tabDictionarySpinBox range.
            editable: true
            from: 0
            to: 1000
            value: root.client.tabCompletionDictionarySize
            onValueModified: root.client.tabCompletionDictionarySize = value
        }
    }

    CheckBox {
        text: qsTr("Clear input on enter")
        checked: root.client.clearInputOnEnter
        onToggled: root.client.clearInputOnEnter = checked
    }
    CheckBox {
        text: qsTr("Auto-resize terminal")
        checked: root.client.autoResizeTerminal
        onToggled: root.client.autoResizeTerminal = checked
    }
    CheckBox {
        text: qsTr("Audible bell")
        checked: root.client.audibleBell
        onToggled: root.client.audibleBell = checked
    }
    CheckBox {
        text: qsTr("Visual bell")
        checked: root.client.visualBell
        onToggled: root.client.visualBell = checked
    }

    Row {
        spacing: 8
        CheckBox {
            id: sepCheckBox
            text: qsTr("Use command separator")
            checked: root.client.useCommandSeparator
            onToggled: root.client.useCommandSeparator = checked
        }
        TextField {
            width: 40
            enabled: sepCheckBox.checked
            maximumLength: 1
            text: root.client.commandSeparator
            onEditingFinished: {
                if (!root.client.setCommandSeparator(text)) {
                    text = root.client.commandSeparator;
                }
            }
        }
    }
}
