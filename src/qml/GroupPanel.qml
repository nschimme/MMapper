import QtQuick
import QtQuick.Controls

import MMapper

PanelFrame {
    id: root

    // Context properties expected to be set by C++ before this component is
    // instantiated: groupModel (GroupModel, used only for its anyMana
    // property), groupProxyModel (GroupProxyModel, the ListView's model),
    // groupController (GroupController), config (QmlConfig).

    // Matches GroupDelegate::sizeHint()'s "999 / 999" monospace measurement
    // in groupwidget.cpp for the HP/Mana/Moves stat columns.
    TextMetrics {
        id: statMetrics
        font.family: "monospace"
        text: "999 / 999"
    }
    readonly property real statW: statMetrics.advanceWidth + 20
    readonly property real nameW: 140
    readonly property real stateW: 90
    readonly property real rowH: 26
    readonly property int statsVisibleCount: groupModel.anyMana ? 3 : 2
    readonly property real roomW: Math.max(40, width - nameW - stateW - statW * statsVisibleCount)

    // Rebuilt whenever groupModel.anyMana changes so the Mana column
    // appears/disappears and the Room Name column's width (roomW, which
    // itself depends on statsVisibleCount) stays in sync.
    readonly property var headerColumns: {
        var cols = [{
                text: qsTr("Name"),
                width: nameW
            }, {
                text: qsTr("HP"),
                width: statW
            }];
        if (groupModel.anyMana) {
            cols.push({
                text: qsTr("Mana"),
                width: statW
            });
        }
        cols.push({
            text: qsTr("Moves"),
            width: statW
        }, {
            text: qsTr("State"),
            width: stateW
        }, {
            text: qsTr("Room Name"),
            width: roomW
        });
        return cols;
    }

    function openMenuFor(index) {
        contextMenu.rowIndex = index;
        contextMenu.popup();
    }

    // Bordered, rounded stat bar with a centered monospace label; mirrors
    // the "Layer 2/3" bar drawing in GroupDelegate::paint() (groupwidget.cpp).
    component StatBar: Item {
        id: statBar

        property real ratio: 0
        property string label: ""
        property bool low: false
        property color fillColor: "gray"

        Rectangle {
            anchors.fill: parent
            radius: 4
            color: root.panelPalette.window
            border.color: "black"
            border.width: 1
            antialiasing: true

            Rectangle {
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.margins: 1
                width: Math.max(0, (parent.width - 2) * statBar.ratio)
                radius: 4
                color: statBar.fillColor
                antialiasing: true

                // Mirrors the sine-wave alpha pulse (1500ms period, cycling
                // between the delegate's pulseMin=100 and pulseMax up to
                // 255) that GroupDelegate::paint() draws for low HP/Moves.
                opacity: statBar.low ? 0.39 : 1.0
                SequentialAnimation on opacity {
                    running: statBar.low
                    loops: Animation.Infinite
                    alwaysRunToEnd: true
                    NumberAnimation {
                        from: 0.39
                        to: 1.0
                        duration: 750
                        easing.type: Easing.InOutSine
                    }
                    NumberAnimation {
                        from: 1.0
                        to: 0.39
                        duration: 750
                        easing.type: Easing.InOutSine
                    }
                }
            }

            Text {
                anchors.centerIn: parent
                text: statBar.label
                font.family: "monospace"
                color: root.panelPalette.text
            }
        }
    }

    Menu {
        id: contextMenu

        property int rowIndex: -1

        MenuItem {
            text: qsTr("&Center")
            enabled: contextMenu.rowIndex >= 0 && groupController.canCenter(contextMenu.rowIndex)
            onTriggered: groupController.centerOnCharacter(contextMenu.rowIndex)
        }
        MenuItem {
            text: qsTr("&Recolor")
            enabled: contextMenu.rowIndex >= 0
            onTriggered: groupController.recolorCharacter(contextMenu.rowIndex)
        }
    }

    Column {
        id: layout
        anchors.fill: parent
        spacing: 0

        PanelHeaderRow {
            id: headerRow
            width: parent.width
            columns: root.headerColumns
        }

        ListView {
            id: listView
            width: parent.width
            height: Math.max(0, parent.height - headerRow.height)
            clip: true
            model: groupProxyModel

            ScrollBar.vertical: ScrollBar {}

            delegate: Rectangle {
                id: delegateRoot
                width: ListView.view.width
                height: rowH
                color: model.charColor

                // Row index (in groupProxyModel) this delegate currently
                // represents; read by DropArea.onDropped below to figure out
                // which two rows to swap via groupController.moveCharacter().
                property int dragIndex: index

                Row {
                    anchors.fill: parent

                    Text {
                        width: nameW
                        height: rowH
                        leftPadding: 4
                        verticalAlignment: Text.AlignVCenter
                        elide: Text.ElideRight
                        text: model.name
                        color: model.textColor
                    }
                    StatBar {
                        width: statW
                        height: rowH
                        ratio: model.hpRatio
                        label: model.hpText
                        low: model.hpLow
                        fillColor: model.hpLow ? "#FF5555" : "#50FA7B"
                    }
                    Item {
                        width: statW
                        height: rowH
                        visible: groupModel.anyMana

                        StatBar {
                            anchors.fill: parent
                            visible: !model.manaHidden
                            ratio: model.manaRatio
                            label: model.manaText
                            fillColor: "#8BE9FD"
                        }
                        Text {
                            anchors.centerIn: parent
                            visible: model.manaHidden
                            text: "--"
                            color: root.panelPalette.text
                        }
                    }
                    StatBar {
                        width: statW
                        height: rowH
                        ratio: model.movesRatio
                        label: model.movesText
                        low: model.movesLow
                        fillColor: "#FFB86C"
                    }
                    Row {
                        width: stateW
                        height: rowH

                        Repeater {
                            model: model.stateIcons
                            Image {
                                source: modelData
                                width: 18
                                height: 18
                                fillMode: Image.PreserveAspectFit
                                ToolTip.visible: false
                            }
                        }
                    }
                    Text {
                        width: Math.max(0, delegateRoot.width - nameW - statW * root.statsVisibleCount - stateW)
                        height: rowH
                        leftPadding: 4
                        verticalAlignment: Text.AlignVCenter
                        elide: Text.ElideRight
                        text: model.roomName
                        color: model.textColor
                        ToolTip.visible: false
                    }
                }

                TapHandler {
                    acceptedButtons: Qt.RightButton
                    onTapped: root.openMenuFor(index)
                    onLongPressed: root.openMenuFor(index)
                }

                // Drag-to-reorder. Deliberately restricted to the left mouse
                // button (DragHandler's default) so it does not compete with
                // the right-click/long-press context menu above.
                DragHandler {
                    id: dragHandler
                    target: null
                    onActiveChanged: delegateRoot.z = active ? 10 : 0
                }
                Drag.active: dragHandler.active
                Drag.source: delegateRoot
                Drag.hotSpot.x: width / 2
                Drag.hotSpot.y: height / 2

                DropArea {
                    anchors.fill: parent
                    onDropped: function (drop) {
                        if (drop.source && drop.source.dragIndex !== undefined) {
                            groupController.moveCharacter(drop.source.dragIndex, index);
                        }
                    }
                }
            }
        }
    }

    Text {
        anchors.centerIn: parent
        visible: listView.count === 0
        text: qsTr("No group members")
        color: root.panelPalette.text
        opacity: 0.5
    }
}
