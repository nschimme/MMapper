import QtQuick
import QtQuick.Controls

import MMapper

PanelFrame {
    id: root

    // Context properties expected to be set by C++ before this component is
    // instantiated: adventureLogModel (AdventureLogModel), config (QmlConfig).

    // Modest minimum, mirroring the panel's widget-era layout minimum
    // (a plain QListView had no dedicated sizeHint() override): tall enough
    // for a couple of wrapped log lines, wide enough to read them.
    implicitWidth: 240
    implicitHeight: 100

    // Rendered as a single read-only selectable text control (rather than
    // per-row ListView delegates) because AdventureLogModel's content is
    // plain text with no per-line virtualization concerns (unlike the
    // scrollback-critical client display) -- this most directly restores
    // AdventureWidget's QTextEdit (Qt::TextSelectableByMouse over the whole
    // document) with a single TextArea bound to adventureLogModel.text.
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
        MenuSeparator {}
        MenuItem {
            text: qsTr("Clear")
            onTriggered: adventureLogModel.clear()
        }
    }

    ScrollView {
        anchors.fill: parent

        TextArea {
            id: textArea
            objectName: "adventureLogTextArea"
            width: parent.width
            readOnly: true
            selectByMouse: true
            wrapMode: TextEdit.Wrap
            text: adventureLogModel.text

            // Mirrors AdventureWidget's ctor: integratedClient background/
            // foreground/font (adventurewidget.cpp:19-42), the same theming
            // ClientDisplay.qml/ClientPanel.qml use via `config`.
            color: config.clientFgColor
            font.family: config.clientFontFamily
            font.pointSize: config.clientFontPointSize > 0 ? config.clientFontPointSize : 10

            background: Rectangle {
                color: config.clientBgColor
            }

            // Force-scroll to bottom on every new message, matching the
            // widget-era ListView behavior this replaces.
            onTextChanged: cursorPosition = length

            // Right-click only (unlike the ListView-based panels' extra
            // long-press handler): a long-press TapHandler here would race
            // with TextArea's own left-button drag-to-select gesture.
            TapHandler {
                acceptedButtons: Qt.RightButton
                onTapped: contextMenu.popup()
            }
        }
    }
}
