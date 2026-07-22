import QtQuick
import QtQuick.Controls

import MMapper

PanelFrame {
    id: root

    // Context properties expected to be set by C++ before this component is
    // instantiated: logModel (LogModel).

    // Modest minimum, mirroring the panel's widget-era layout minimum (a
    // plain QListView had no dedicated sizeHint() override): tall enough
    // for a couple of wrapped log lines, wide enough to read them.
    implicitWidth: 240
    implicitHeight: 100

    // Rendered as a single read-only selectable text control (rather than
    // per-row ListView delegates): the widget original was a QTextBrowser
    // with default (unthemed) interaction flags, so a single TextArea bound
    // to logModel.text most directly restores drag-select + copy of
    // arbitrary text, matching mainwindow.cpp's "logWindow".
    Menu {
        id: contextMenu

        MenuItem {
            text: qsTr("Copy")
            enabled: textArea.selectedText.length > 0
            onTriggered: textArea.copy()
        }
        MenuItem {
            text: qsTr("Select All")
            onTriggered: textArea.selectAll()
        }
        MenuItem {
            text: qsTr("Copy All")
            onTriggered: logModel.copyAll()
        }
        MenuSeparator {}
        MenuItem {
            text: qsTr("Clear")
            onTriggered: logModel.clear()
        }
    }

    ScrollView {
        anchors.fill: parent
        visible: textArea.text.length > 0

        TextArea {
            id: textArea
            objectName: "logTextArea"
            width: parent.width
            readOnly: true
            selectByMouse: true
            wrapMode: TextEdit.Wrap
            textFormat: TextEdit.PlainText
            text: logModel.text
            color: root.panelPalette.text

            // The widget-based Log Panel always force-scrolls to the bottom
            // on every new message, so match that behavior exactly.
            onTextChanged: cursorPosition = length

            // Right-click only: a long-press TapHandler here would race with
            // TextArea's own left-button drag-to-select gesture.
            TapHandler {
                acceptedButtons: Qt.RightButton
                onTapped: contextMenu.popup()
            }
        }
    }

    Text {
        anchors.centerIn: parent
        visible: textArea.text.length === 0
        text: qsTr("No log messages")
        color: root.panelPalette.text
        opacity: 0.5
    }
}
