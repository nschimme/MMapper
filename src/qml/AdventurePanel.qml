import QtQuick
import QtQuick.Controls

import MMapper

PanelFrame {
    id: root

    // Context properties expected to be set by C++ before this component is
    // instantiated: adventureLogModel (AdventureLogModel).

    // Modest minimum, mirroring the panel's widget-era layout minimum
    // (a plain QListView had no dedicated sizeHint() override): tall enough
    // for a couple of wrapped log lines, wide enough to read them.
    implicitWidth: 240
    implicitHeight: 100

    Menu {
        id: contextMenu

        MenuItem {
            text: qsTr("Clear")
            onTriggered: adventureLogModel.clear()
        }
    }

    ListView {
        id: listView
        anchors.fill: parent
        clip: true
        model: adventureLogModel

        ScrollBar.vertical: ScrollBar {}

        // The widget-based Adventure Panel always force-scrolls to the
        // bottom on every new message, so match that behavior exactly.
        onCountChanged: positionViewAtEnd()

        delegate: Text {
            width: ListView.view.width
            text: model.display
            wrapMode: Text.Wrap
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
}
