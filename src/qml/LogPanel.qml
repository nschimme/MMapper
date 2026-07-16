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

    Menu {
        id: contextMenu

        MenuItem {
            text: qsTr("Clear")
            onTriggered: logModel.clear()
        }
        MenuItem {
            text: qsTr("Copy All")
            onTriggered: logModel.copyAll()
        }
    }

    ListView {
        id: listView
        anchors.fill: parent
        clip: true
        model: logModel

        ScrollBar.vertical: ScrollBar {}

        // The widget-based Log Panel always force-scrolls to the bottom on
        // every new message, so match that behavior exactly.
        onCountChanged: positionViewAtEnd()

        delegate: Text {
            width: ListView.view.width
            text: model.display
            wrapMode: Text.Wrap
            textFormat: Text.PlainText
            color: root.panelPalette.text
        }

        TapHandler {
            acceptedButtons: Qt.RightButton
            onTapped: contextMenu.popup()
        }

        TapHandler {
            acceptedButtons: Qt.LeftButton
            onLongPressed: contextMenu.popup()
        }
    }

    Text {
        anchors.centerIn: parent
        visible: listView.count === 0
        text: qsTr("No log messages")
        color: root.panelPalette.text
        opacity: 0.5
    }
}
