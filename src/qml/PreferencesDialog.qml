// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

import QtQuick
import QtQuick.Controls

import MMapper

// The Preferences dialog's QML shell, driven by PreferencesController (see
// ../preferences/PreferencesController.h) exposed as the
// "preferencesController" context property, and "dialog" (the enclosing
// QmlDialog, see QmlDialog.h) for the OK/Cancel buttons. Mirrors
// configdialog.ui's left-nav-plus-page layout (minus search) without a .ui
// file.
//
// This round (Phase 6, Commits 10-11 of the QML migration) only ports the
// five SIMPLE pages that PreferencesController exposes adapters for; the
// other four ConfigDialog pages (General, Graphics, Parser, Integrated
// Client) have no QML page yet, so the nav below deliberately lists only
// these five. See PreferencesController.h for why MainWindow does not yet
// switch to this dialog.
Item {
    id: root
    implicitWidth: 800
    implicitHeight: 600

    // Each entry's "source" is a module-relative qrc path, resolved the same
    // way QmlDialog::setQmlSource() resolves top-level dialog sources.
    ListModel {
        id: navModel
        ListElement { label: qsTr("Path Machine"); source: "qrc:/qt/qml/MMapper/PrefsPathMachinePage.qml" }
        ListElement { label: qsTr("Mume Protocol"); source: "qrc:/qt/qml/MMapper/PrefsMumeProtocolPage.qml" }
        ListElement { label: qsTr("Auto Logger"); source: "qrc:/qt/qml/MMapper/PrefsAutoLogPage.qml" }
        ListElement { label: qsTr("Group Manager"); source: "qrc:/qt/qml/MMapper/PrefsGroupPage.qml" }
        ListElement { label: qsTr("Audio"); source: "qrc:/qt/qml/MMapper/PrefsAudioPage.qml" }
    }

    SystemPalette {
        id: sysPalette
        colorGroup: SystemPalette.Active
    }

    Rectangle {
        id: navFrame
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.bottom: footerRow.top
        anchors.margins: 8
        anchors.bottomMargin: 8
        width: 180
        color: sysPalette.base
        border.color: sysPalette.mid
        border.width: 1

        ListView {
            id: navList
            objectName: "preferencesNavList"
            anchors.fill: parent
            anchors.margins: 1
            clip: true
            model: navModel
            currentIndex: 0

            delegate: Rectangle {
                width: navList.width
                height: 32
                color: navList.currentIndex === index ? sysPalette.highlight : "transparent"

                Text {
                    anchors.fill: parent
                    anchors.leftMargin: 8
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                    text: model.label
                    color: navList.currentIndex === index ? sysPalette.highlightedText : sysPalette.text
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: navList.currentIndex = index
                }
            }
        }
    }

    Loader {
        id: pageLoader
        objectName: "preferencesPageLoader"
        anchors.top: parent.top
        anchors.left: navFrame.right
        anchors.right: parent.right
        anchors.bottom: footerRow.top
        anchors.margins: 8
        anchors.leftMargin: 8
        source: navModel.get(navList.currentIndex).source
    }

    Row {
        id: footerRow
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        anchors.margins: 8
        spacing: 8

        Button {
            id: cancelButton
            text: qsTr("Cancel")
            onClicked: {
                preferencesController.cancel();
                dialog.reject();
            }
        }

        Button {
            id: okButton
            text: qsTr("OK")
            onClicked: {
                preferencesController.ok();
                dialog.accept();
            }
        }
    }
}
