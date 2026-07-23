import QtQuick
import QtQuick.Controls

import MMapper

PanelFrame {
    id: root

    // Context properties expected to be set by C++ before this component is
    // instantiated: tasksModel (TasksModel).

    // Modest minimum, mirroring the panel's widget-era layout minimum (a
    // plain QListView had no dedicated sizeHint() override): tall enough
    // for one task's status text + progress bar + cancel button, wide
    // enough for the button's label.
    implicitWidth: 260
    implicitHeight: 110

    function openMenuFor(index) {
        contextMenu.rowIndex = index;
        contextMenu.popup();
    }

    Menu {
        id: contextMenu

        property int rowIndex: -1

        MenuItem {
            implicitHeight: Theme.controlHeight
            // Mirrors TasksPanel::contextMenuClick() in TasksPanel.cpp: the
            // entry is always offered (not gated on cancelability here), and
            // cancelTask() itself is a no-op if the task doesn't allow it.
            text: qsTr("Cancel")
            enabled: contextMenu.rowIndex >= 0
            onTriggered: tasksModel.cancelTask(contextMenu.rowIndex)
        }
    }

    ListView {
        id: listView
        anchors.fill: parent
        clip: true
        model: tasksModel

        ScrollBar.vertical: ScrollBar {}

        // Mirrors TasksPanel::refresh_data()'s "!underMouse()" removal
        // suppression: while the pointer is hovering the list, finished
        // tasks stay in the model (still refreshed in place) instead of
        // being yanked out from under the user.
        HoverHandler {
            id: hoverHandler
            onHoveredChanged: tasksModel.holdRemovals = hovered
        }

        delegate: Column {
            id: delegateRoot
            width: ListView.view.width
            spacing: 4
            leftPadding: 4
            rightPadding: 4
            topPadding: 4
            bottomPadding: 4

            Text {
                width: delegateRoot.width - delegateRoot.leftPadding - delegateRoot.rightPadding
                text: model.statusText
                wrapMode: Text.Wrap
                color: root.panelPalette.text
            }

            Rectangle {
                width: delegateRoot.width - delegateRoot.leftPadding - delegateRoot.rightPadding
                height: 6
                radius: 3
                color: root.panelPalette.base
                border.color: root.panelPalette.mid
                border.width: 1

                Rectangle {
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    anchors.margins: 1
                    width: Math.max(0, (parent.width - 2) * (model.percent / 100))
                    radius: 3
                    color: root.panelPalette.highlight
                }
            }

            Button {
                implicitHeight: Theme.controlHeight
                text: qsTr("Cancel Task #%1").arg(model.taskId)
                enabled: model.canCancel
                onClicked: tasksModel.cancelTask(index)
            }

            TapHandler {
                acceptedButtons: Qt.RightButton
                onTapped: root.openMenuFor(index)
                onLongPressed: root.openMenuFor(index)
            }
        }
    }

    Text {
        anchors.centerIn: parent
        visible: listView.count === 0
        text: qsTr("No running tasks")
        color: root.panelPalette.text
        opacity: 0.5
    }
}
