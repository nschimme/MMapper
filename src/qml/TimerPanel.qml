import QtQuick
import QtQuick.Controls

import MMapper

PanelFrame {
    id: root

    // Context properties expected to be set by C++ before this component is
    // instantiated: timerModel (TimerModel), timerController (TimerController).

    function openMenuFor(index) {
        contextMenu.rowIndex = index;
        contextMenu.popup();
    }

    Menu {
        id: contextMenu

        property int rowIndex: -1

        MenuItem {
            text: qsTr("Reset")
            onTriggered: timerController.reset(contextMenu.rowIndex)
        }
        MenuItem {
            text: qsTr("Stop")
            onTriggered: timerController.stop(contextMenu.rowIndex)
        }
        MenuItem {
            text: qsTr("Delete")
            onTriggered: timerController.remove(contextMenu.rowIndex)
        }
        MenuItem {
            text: qsTr("Clear Expired")
            onTriggered: timerController.clearExpired()
        }
    }

    ListView {
        id: listView
        anchors.fill: parent
        clip: true
        model: timerModel

        ScrollBar.vertical: ScrollBar {}

        delegate: Rectangle {
            id: delegateRoot
            width: ListView.view.width
            height: 30
            color: index % 2 === 0 ? root.panelPalette.base
                                    : root.panelPalette.alternateBase

            Rectangle {
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                width: parent.width * (model.progress ? model.progress : 0)
                color: root.panelPalette.highlight
                opacity: 0.3
            }

            Text {
                anchors.left: parent.left
                anchors.right: timeText.left
                anchors.verticalCenter: parent.verticalCenter
                anchors.leftMargin: 4
                anchors.rightMargin: 4
                text: model.name
                elide: Text.ElideRight
                color: model.expired ? "red" : root.panelPalette.text
            }

            Text {
                id: timeText
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.rightMargin: 4
                text: model.time
                color: model.expired ? "red" : root.panelPalette.text
            }

            TapHandler {
                acceptedButtons: Qt.RightButton
                onTapped: root.openMenuFor(index)
                onLongPressed: root.openMenuFor(index)
            }

            TapHandler {
                acceptedButtons: Qt.LeftButton
                onLongPressed: root.openMenuFor(index)
            }
        }
    }

    Text {
        anchors.centerIn: parent
        visible: listView.count === 0
        text: qsTr("No timers")
        color: root.panelPalette.text
        opacity: 0.5
    }
}
