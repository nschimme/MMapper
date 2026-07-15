// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

import QtQuick
import QtQuick.Controls

import MMapper

// Group Manager preferences page, driven by preferencesController.group (a
// GroupPageAdapter, see ../preferences/GroupPageAdapter.h). Mirrors
// grouppage.ui's Appearance / Filtering and Order group boxes without a .ui
// file. Color swatches are plain Rectangles (mirroring the widget page's
// icon-on-button swatch) since there is no packaged-module color-picker
// button; the native QColorDialog is opened via the adapter's
// chooseColor()/chooseNpcOverrideColor() invokables.
Flickable {
    id: root
    clip: true
    contentWidth: width
    contentHeight: column.implicitHeight

    readonly property var group: preferencesController.group

    Column {
        id: column
        width: root.width
        spacing: 8

        Label { text: qsTr("Appearance"); font.bold: true }

        Row {
            spacing: 8
            Label { text: qsTr("Your color:"); width: 160 }
            Rectangle {
                width: 24
                height: 16
                border.color: "black"
                border.width: 1
                color: root.group.color
            }
            Button {
                text: qsTr("Select")
                onClicked: root.group.chooseColor()
            }
        }

        Row {
            spacing: 8
            CheckBox {
                id: npcOverrideColorCheckBox
                checked: root.group.npcColorOverride
                onToggled: root.group.npcColorOverride = checked
            }
            Label { text: qsTr("Override NPC color:"); width: 148; anchors.verticalCenter: parent.verticalCenter }
            Rectangle {
                width: 24
                height: 16
                border.color: "black"
                border.width: 1
                color: root.group.npcColor
            }
            Button {
                text: qsTr("Select")
                onClicked: root.group.chooseNpcOverrideColor()
            }
        }

        Label { text: qsTr("Filtering and Order"); font.bold: true }

        CheckBox {
            id: npcSortBottomCheckBox
            text: qsTr("Sort NPCs to bottom")
            checked: root.group.npcSortBottom
            onToggled: root.group.npcSortBottom = checked
        }

        CheckBox {
            id: npcHideCheckBox
            text: qsTr("Hide NPCs")
            checked: root.group.npcHide
            onToggled: root.group.npcHide = checked
        }
    }
}
