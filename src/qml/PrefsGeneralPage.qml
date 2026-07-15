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
Flickable {
    id: root
    clip: true
    contentWidth: width
    contentHeight: column.implicitHeight

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

    Column {
        id: column
        width: root.width
        spacing: 8

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
                value: root.general.localPort
                onValueModified: root.general.localPort = value
            }
        }
        CheckBox {
            text: qsTr("TLS encryption")
            enabled: root.general.tlsAvailable
            checked: root.general.tlsEncryption
            onToggled: root.general.tlsEncryption = checked
        }
        CheckBox {
            text: qsTr("Proxy listens on any interface")
            checked: root.general.proxyListensOnAnyInterface
            onToggled: root.general.proxyListensOnAnyInterface = checked
        }
        CheckBox {
            text: qsTr("Show proxy connection status")
            checked: root.general.proxyConnectionStatus
            onToggled: root.general.proxyConnectionStatus = checked
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
            Label {
                width: 260
                elide: Text.ElideMiddle
                text: root.general.autoLoadFileName
                anchors.verticalCenter: parent.verticalCenter
            }
            Button {
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
            Label {
                width: 220
                elide: Text.ElideMiddle
                text: root.general.resourcesDirectory
                anchors.verticalCenter: parent.verticalCenter
            }
            Button {
                text: qsTr("Browse")
                onClicked: root.general.chooseResourcesDirectory()
            }
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
}
