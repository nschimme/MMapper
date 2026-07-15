// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

import QtQuick
import QtQuick.Controls

import MMapper

// Mume Protocol preferences page, driven by
// preferencesController.mumeProtocol (a MumeProtocolPageAdapter, see
// ../preferences/MumeProtocolPageAdapter.h). Mirrors mumeprotocolpage.ui's
// remote-editing radio group without a .ui file.
Flickable {
    id: root
    clip: true
    contentWidth: width
    contentHeight: column.implicitHeight

    readonly property var mumeProtocol: preferencesController.mumeProtocol

    Column {
        id: column
        width: root.width
        spacing: 8

        Label { text: qsTr("Remote Editing"); font.bold: true }

        ButtonGroup { id: editorGroup }

        RadioButton {
            id: internalEditorRadio
            text: qsTr("Internal editor")
            checked: root.mumeProtocol.internalRemoteEditor
            ButtonGroup.group: editorGroup
            onToggled: root.mumeProtocol.internalRemoteEditor = checked
        }

        RadioButton {
            id: externalEditorRadio
            text: qsTr("External editor:")
            checked: !root.mumeProtocol.internalRemoteEditor
            ButtonGroup.group: editorGroup
        }

        Row {
            spacing: 8
            enabled: !root.mumeProtocol.internalRemoteEditor

            TextField {
                id: externalEditorCommandField
                width: 260
                text: root.mumeProtocol.externalRemoteEditorCommand
                onEditingFinished: root.mumeProtocol.externalRemoteEditorCommand = text
            }

            Button {
                id: browseButton
                text: qsTr("Browse")
                onClicked: root.mumeProtocol.browseForEditor()
            }
        }
    }
}
