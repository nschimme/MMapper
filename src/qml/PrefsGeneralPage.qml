// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

import QtQuick
import QtQuick.Controls

import MMapper

// General preferences page, driven by preferencesController.general (a
// GeneralPageAdapter, see ../preferences/GeneralPageAdapter.h). Mirrors
// generalpage.ui's Connection/Mume/Startup/Interface/Account/Configuration
// group boxes without a .ui file. Every native dialog (world-file picker,
// resource-directory picker, password management, factory reset confirm,
// import/export) stays native, launched via the adapter's invokables.
Column {
    id: root
    spacing: 8

    readonly property var general: preferencesController.general

    ListModel {
        id: charsetModel
        ListElement { text: "Latin-1" }
        ListElement { text: "UTF-8" }
        ListElement { text: "ASCII" }
    }
    ListModel {
        id: themeModel
        ListElement { text: "System" }
        ListElement { text: "Dark" }
        ListElement { text: "Light" }
    }

    Label { text: qsTr("Connection"); font.bold: true }

    Row {
        spacing: 8
        Label { text: qsTr("Remote host:"); width: 160 }
        TextField {
            width: 220
            text: root.general.remoteName
            onEditingFinished: root.general.remoteName = text
        }
    }
    Row {
        spacing: 8
        Label { text: qsTr("Remote port:"); width: 160 }
        SpinBox {
            from: 0
            to: 65535
            editable: true
            value: root.general.remotePort
            onValueModified: root.general.remotePort = value
        }
    }
    Row {
        spacing: 8
        Label { text: qsTr("Local port:"); width: 160 }
        SpinBox {
            from: 0
            to: 65535
            editable: true
            value: root.general.localPort
            onValueModified: root.general.localPort = value
        }
    }
    CheckBox {
        text: qsTr("TLS encryption")
        enabled: root.general.tlsAvailable
        checked: root.general.tlsEncryption
        onToggled: root.general.tlsEncryption = checked
        ToolTip.text: qsTr("Require that your connection to MUME is always secure")
        ToolTip.visible: hovered
    }
    CheckBox {
        text: qsTr("Proxy listens on any interface")
        checked: root.general.proxyListensOnAnyInterface
        onToggled: root.general.proxyListensOnAnyInterface = checked
        ToolTip.text: qsTr("Allow external traffic to connect to MMapper")
        ToolTip.visible: hovered
    }
    CheckBox {
        text: qsTr("Show proxy connection status")
        checked: root.general.proxyConnectionStatus
        onToggled: root.general.proxyConnectionStatus = checked
        ToolTip.text: qsTr("Disconnect from the mud client when MUME disconnects")
        ToolTip.visible: hovered
    }
    Row {
        spacing: 8
        Label { text: qsTr("Character encoding:"); width: 160 }
        ComboBox {
            width: 120
            model: charsetModel
            textRole: "text"
            currentIndex: root.general.characterEncodingIndex
            onActivated: index => root.general.characterEncodingIndex = index
        }
    }
    Row {
        spacing: 8
        Label { text: qsTr("Theme:"); width: 160 }
        ComboBox {
            width: 120
            model: themeModel
            textRole: "text"
            currentIndex: root.general.themeIndex
            onActivated: index => root.general.themeIndex = index
        }
    }

    Label { text: qsTr("Mume Native"); font.bold: true }

    CheckBox {
        text: qsTr("Emulated exits")
        checked: root.general.emulatedExits
        onToggled: root.general.emulatedExits = checked
    }
    CheckBox {
        text: qsTr("Show hidden exit flags")
        checked: root.general.showHiddenExitFlags
        onToggled: root.general.showHiddenExitFlags = checked
    }
    CheckBox {
        text: qsTr("Show notes")
        checked: root.general.showNotes
        onToggled: root.general.showNotes = checked
    }
    CheckBox {
        text: qsTr("Check for update on startup")
        enabled: root.general.updaterAvailable
        checked: root.general.checkForUpdate
        onToggled: root.general.checkForUpdate = checked
    }

    Label { text: qsTr("Startup"); font.bold: true }

    Row {
        spacing: 8
        CheckBox {
            id: autoLoadCheckBox
            text: qsTr("Auto-load map:")
            checked: root.general.autoLoadMap
            onToggled: root.general.autoLoadMap = checked
        }
        TextField {
            width: 260
            enabled: autoLoadCheckBox.checked
            placeholderText: qsTr("Default Map")
            text: root.general.autoLoadFileName
            onEditingFinished: root.general.autoLoadFileName = text
            anchors.verticalCenter: parent.verticalCenter
        }
        Button {
            enabled: autoLoadCheckBox.checked && !root.general.isWasm
            text: qsTr("Browse")
            onClicked: root.general.selectWorldFile()
        }
    }
    CheckBox {
        text: qsTr("Display MUME clock")
        checked: root.general.displayMumeClock
        onToggled: root.general.displayMumeClock = checked
    }
    CheckBox {
        text: qsTr("Display XP status")
        checked: root.general.displayXPStatus
        onToggled: root.general.displayXPStatus = checked
    }

    Label { text: qsTr("Resources"); font.bold: true }

    Row {
        spacing: 8
        Label { text: qsTr("Resources directory:"); width: 160 }
        TextField {
            width: 220
            enabled: !root.general.isWasm
            placeholderText: qsTr("Path to resources")
            text: root.general.resourcesDirectory
            onEditingFinished: root.general.resourcesDirectory = text
            anchors.verticalCenter: parent.verticalCenter
        }
        Button {
            enabled: !root.general.isWasm
            text: qsTr("Browse")
            onClicked: root.general.chooseResourcesDirectory()
        }
    }

    Label { text: qsTr("Shell"); font.bold: true }

    CheckBox {
        text: qsTr("Use the QML shell (requires restart)")
        checked: root.general.qmlShell
        onToggled: root.general.qmlShell = checked
    }
    Label {
        width: 460
        wrapMode: Text.WordWrap
        font.italic: true
        text: qsTr("Switches between the classic widgets-based shell and the "
                    + "newer QML shell. Only takes effect the next time MMapper "
                    + "starts; can also be overridden with --qml-shell / "
                    + "--widgets-shell on the command line.")
    }

    Label { text: qsTr("Account"); font.bold: true }

    CheckBox {
        text: qsTr("Remember login")
        enabled: root.general.keychainAvailable && root.general.autoLoginEnabled
        checked: root.general.rememberLogin
        onToggled: root.general.rememberLogin = checked
    }
    Row {
        spacing: 8
        Button {
            enabled: root.general.keychainAvailable
            text: root.general.hasPassword ? qsTr("Manage Password") : qsTr("Set Password")
            onClicked: root.general.managePassword()
        }
    }

    Label { text: qsTr("Configuration"); font.bold: true }

    Row {
        spacing: 8
        Button {
            text: qsTr("Factory Reset")
            onClicked: root.general.factoryReset()
        }
        Button {
            text: qsTr("Export")
            onClicked: root.general.exportConfig()
        }
        Button {
            text: qsTr("Import")
            onClicked: root.general.importConfig()
        }
    }
}
