// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

import QtQuick
import QtQuick.Controls

import MMapper

// Parser preferences page, driven by preferencesController.parser (a
// ParserPageAdapter, see ../preferences/ParserPageAdapter.h). Mirrors
// parserpage.ui's Offline Emulation / Commands / Emoji Shortcodes group
// boxes without a .ui file. The room name/description ANSI color pickers
// stay native (AnsiColorDialog, launched via the adapter's
// chooseRoomNameColor()/chooseRoomDescColor() invokables) rather than being
// re-implemented in QML; see ParserPageAdapter.h for why.
Column {
    id: root
    spacing: 8

    readonly property var parser: preferencesController.parser

    component AnsiColorRow: Row {
        property alias label: rowLabel.text
        property color fgColor
        property color bgColor
        property string previewText
        signal chooseRequested()
        spacing: 8
        Label { id: rowLabel; width: 140 }
        Rectangle {
            width: 90
            height: 20
            border.color: "black"
            border.width: 1
            color: bgColor
            anchors.verticalCenter: parent.verticalCenter
            Text {
                anchors.centerIn: parent
                text: previewText
                color: fgColor
            }
        }
        Button {
            text: qsTr("Select")
            onClicked: chooseRequested()
        }
    }

    Label { text: qsTr("Offline Emulation"); font.bold: true }

    AnsiColorRow {
        label: qsTr("Room name color:")
        fgColor: root.parser.roomNameColorFg
        bgColor: root.parser.roomNameColorBg
        previewText: qsTr("roomname")
        onChooseRequested: root.parser.chooseRoomNameColor()
    }

    AnsiColorRow {
        label: qsTr("Descriptions color:")
        fgColor: root.parser.roomDescColorFg
        bgColor: root.parser.roomDescColorBg
        previewText: qsTr("roomdesc")
        onChooseRequested: root.parser.chooseRoomDescColor()
    }

    Label { text: qsTr("Commands"); font.bold: true }

    Row {
        spacing: 8
        Label { text: qsTr("Character prefix:"); width: 140 }
        TextField {
            id: prefixField
            width: 40
            maximumLength: 1
            text: root.parser.prefixChar
            onEditingFinished: {
                if (!root.parser.setPrefixChar(text)) {
                    // Revert to the last valid value, mirroring
                    // CommandPrefixValidator rejecting the keystroke.
                    text = root.parser.prefixChar;
                }
            }
        }
    }

    Label { text: qsTr("Emoji Shortcodes"); font.bold: true }

    CheckBox {
        text: qsTr("Encode to shortcodes")
        checked: root.parser.encodeEmoji
        onToggled: root.parser.encodeEmoji = checked
    }

    CheckBox {
        text: qsTr("Decode from shortcodes")
        checked: root.parser.decodeEmoji
        onToggled: root.parser.decodeEmoji = checked
    }
}
