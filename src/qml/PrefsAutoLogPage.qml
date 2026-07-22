// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

import QtQuick
import QtQuick.Controls

import MMapper

// Auto Logger preferences page, driven by preferencesController.autoLog (an
// AutoLogPageAdapter, see ../preferences/AutoLogPageAdapter.h). Mirrors
// autologpage.ui's three group boxes (Automatic Logging, Automatic Log
// Cleanup, Advanced) without a .ui file.
//
// cleanupStrategy is an int matching AutoLoggerEnum's ordinal (see
// AutoLogPageAdapter.h): KeepForever=0, DeleteDays=1, DeleteSize=2.
Column {
    id: root
    spacing: 8

    readonly property var autoLog: preferencesController.autoLog

    Label { text: qsTr("Automatic Logging"); font.bold: true }

    CheckBox {
        id: autoLogCheckBox
        text: qsTr("Automatically log play sessions to disk")
        checked: root.autoLog.autoLog
        onToggled: root.autoLog.autoLog = checked
    }

    Row {
        spacing: 8
        enabled: autoLogCheckBox.checked

        Label { text: qsTr("Log location:") }

        TextField {
            id: autoLogLocationField
            width: 260
            text: root.autoLog.autoLogDirectory
            onEditingFinished: root.autoLog.autoLogDirectory = text
        }

        Button {
            enabled: !root.autoLog.isWasm
            text: qsTr("Select")
            onClicked: root.autoLog.browseForDirectory()
        }
    }

    Label { text: qsTr("Automatic Log Cleanup"); font.bold: true }

    ButtonGroup { id: cleanupGroup }

    RadioButton {
        text: qsTr("Keep logs forever")
        checked: root.autoLog.cleanupStrategy === 0
        ButtonGroup.group: cleanupGroup
        onToggled: if (checked) root.autoLog.cleanupStrategy = 0
    }

    Row {
        spacing: 8
        RadioButton {
            id: deleteDaysRadio
            text: qsTr("Delete logs after")
            checked: root.autoLog.cleanupStrategy === 1
            ButtonGroup.group: cleanupGroup
            onToggled: if (checked) root.autoLog.cleanupStrategy = 1
        }
        SpinBox {
            id: deleteDaysBox
            editable: true
            from: 0
            to: 9999999
            enabled: deleteDaysRadio.checked
            value: root.autoLog.deleteWhenLogsReachDays
            onValueModified: root.autoLog.deleteWhenLogsReachDays = value
        }
        Label { text: qsTr("days"); anchors.verticalCenter: parent.verticalCenter }
    }

    Row {
        spacing: 8
        RadioButton {
            id: deleteSizeRadio
            text: qsTr("Delete logs after")
            checked: root.autoLog.cleanupStrategy === 2
            ButtonGroup.group: cleanupGroup
            onToggled: if (checked) root.autoLog.cleanupStrategy = 2
        }
        SpinBox {
            id: deleteSizeBox
            editable: true
            from: 1
            to: 10000
            enabled: deleteSizeRadio.checked
            value: root.autoLog.deleteWhenLogsReachMB
            onValueModified: root.autoLog.deleteWhenLogsReachMB = value
        }
        Label { text: qsTr("MBs"); anchors.verticalCenter: parent.verticalCenter }
    }

    CheckBox {
        id: askDeleteCheckBox
        text: qsTr("Always ask before deleting logs")
        checked: root.autoLog.askDelete
        onToggled: root.autoLog.askDelete = checked
    }

    Label { text: qsTr("Advanced"); font.bold: true }

    Row {
        spacing: 8
        Label { text: qsTr("Rotate logs after") }
        SpinBox {
            id: rotateBox
            editable: true
            from: 1
            to: 1000
            value: root.autoLog.rotateWhenLogsReachMB
            onValueModified: root.autoLog.rotateWhenLogsReachMB = value
        }
        Label { text: qsTr("MBs"); anchors.verticalCenter: parent.verticalCenter }
    }
}
