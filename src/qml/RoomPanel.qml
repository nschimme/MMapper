import QtQuick
import QtQuick.Controls

import MMapper

PanelFrame {
    id: root

    // Context property expected to be set by C++ before this component is
    // instantiated: roomModel (RoomModel).

    color: root.panelPalette.base

    Column {
        anchors.fill: parent
        spacing: 0

        HorizontalHeaderView {
            id: headerView
            width: parent.width
            syncView: tableView
            clip: true
        }

        TableView {
            id: tableView
            width: parent.width
            height: parent.height - headerView.height
            clip: true
            model: roomModel
            columnSpacing: 0
            rowSpacing: 0

            ScrollBar.vertical: ScrollBar {}
            ScrollBar.horizontal: ScrollBar {}

            delegate: Rectangle {
                id: cellDelegate

                implicitWidth: cellText.implicitWidth + 12
                implicitHeight: 26
                color: model.rowBackground !== undefined && model.rowBackground
                       ? model.rowBackground : "transparent"

                Text {
                    id: cellText
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.left: parent.left
                    anchors.leftMargin: 6
                    text: model.display !== undefined ? model.display : ""
                    color: model.rowForeground !== undefined && model.rowForeground
                           ? model.rowForeground : root.panelPalette.text
                }
            }
        }
    }
}
