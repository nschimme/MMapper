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
    readonly property real rowH: Theme.rowHeight
    readonly property int statsVisibleCount: groupModel.anyMana ? 3 : 2
    // 120px keeps the Room Name column wide enough to show a handful of
    // characters (this is the single most useful field for "who's where");
    // below that it stops shrinking and the row simply becomes wider than
    // the viewport instead of collapsing to an unreadable sliver. On a wide
    // (desktop) panel this still behaves exactly like before: the Math.max
    // is never hit, roomW absorbs all the slack, and contentW below equals
    // hFlick's own width.
    //
    // Uses hFlick.width (the actual viewport the columns render into, i.e.
    // PanelFrame's contentItem width) rather than root.width, since
    // PanelFrame subtracts its own 4px anchors.margins on every side
    // (PanelFrame.qml) -- using root.width here would overstate the
    // available room by 8px and make contentW spuriously exceed hFlick's
    // width even when the panel is otherwise wide enough to fit everything.
    readonly property real roomW: Math.max(120, hFlick.width - nameW - stateW - statW * statsVisibleCount)

    // Total width of one row (header or data). On desktop-width panels this
    // equals hFlick.width (roomW absorbed the slack above), so hFlick's
    // contentWidth below equals its own width and nothing scrolls -- pixel
    // identical to the previous fixed-Row layout. On a narrow (phone-width)
    // panel roomW floors out at 120 and contentW exceeds hFlick.width,
    // which is exactly what makes the overflow reachable via hFlick instead
    // of clipped/cut off past the right edge.
    readonly property real contentW: nameW + stateW + statW * statsVisibleCount + roomW

    // Mirrors GroupWidget::sizeHint() (groupwidget.cpp): header height plus
    // one data row, width the sum of the (minimum) column widths, so the
    // dock can't be squashed below "header + one member visible" (see
    // QmlDockWidget::syncMinimumSize()).
    implicitWidth: nameW + stateW + statW * statsVisibleCount + 40
    implicitHeight: headerRow.height + rowH

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
        // Mirrors GroupModel::data()'s Qt::ToolTipRole for the HP/Mana/Moves
        // columns (e.g. "42%"), shown by QTableView on hover in the widget;
        // ported here as a hover ToolTip on the bar itself.
        property string toolTip: ""

        ToolTip.text: statBar.toolTip
        ToolTip.visible: statBar.toolTip.length > 0 && hoverHandler.hovered

        HoverHandler {
            id: hoverHandler
        }

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
            implicitHeight: Theme.controlHeight
            text: qsTr("&Center")
            enabled: contextMenu.rowIndex >= 0 && groupController.canCenter(contextMenu.rowIndex)
            onTriggered: groupController.centerOnCharacter(contextMenu.rowIndex)
        }
        MenuItem {
            implicitHeight: Theme.controlHeight
            text: qsTr("&Recolor")
            enabled: contextMenu.rowIndex >= 0
            onTriggered: groupController.recolorCharacter(contextMenu.rowIndex)
        }
    }

    // Wraps the header + list so the fixed-width columns become
    // horizontally swipeable instead of clipped/overflowing off the right
    // edge when the panel is narrower than contentW (e.g. the compact
    // mobile drawer). Mirrors RoomPanel's existing degrade-to-scroll
    // pattern (there via TableView's built-in horizontal ScrollBar; here via
    // an explicit Flickable since this panel's body is a ListView, not a
    // TableView). At desktop widths contentWidth == width, so hFlick has
    // nothing to scroll and the layout renders identically to before this
    // fix.
    Flickable {
        id: hFlick
        objectName: "groupHFlick"
        anchors.fill: parent
        contentWidth: Math.max(width, root.contentW)
        contentHeight: height
        flickableDirection: Flickable.HorizontalFlick
        clip: true
        ScrollBar.horizontal: ScrollBar {}

        Column {
            id: layout
            width: hFlick.contentWidth
            height: hFlick.height
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
                // Only claim VERTICAL drags. A ListView defaults to
                // AutoFlickDirection and, sitting inside hFlick, would grab
                // horizontal drags too (consuming them without moving, since
                // its own contentWidth equals its width), so the horizontal
                // Flickable wrapping it never saw them and the overflowing
                // columns could not be scrolled to by touch.
                flickableDirection: Flickable.VerticalFlick

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

                    // Captured here (rather than read as `model.stateTip` inside
                    // the state-icon Repeater below) because a Repeater delegate
                    // has its own `model` context (the stateIcons array item),
                    // which would shadow this row's `model.stateTip`.
                    property string rowStateTip: model.stateTip ? model.stateTip : ""

                    // Same hazard, one step earlier: `Repeater { model:
                    // model.stateIcons }` is self-referential, because a
                    // Repeater's OWN `model` property shadows the delegate's
                    // model context in that binding's scope -- so it read
                    // .stateIcons off the Repeater's (still undefined) model
                    // and threw "Cannot read property 'stateIcons' of
                    // undefined", leaving the state column empty. Capture the
                    // list here and reference it qualified below.
                    property var rowStateIcons: model.stateIcons

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
                            toolTip: model.hpTip ? model.hpTip : ""
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
                                toolTip: model.manaTip ? model.manaTip : ""
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
                            toolTip: model.movesTip ? model.movesTip : ""
                        }
                        Row {
                            width: stateW
                            height: rowH

                            Repeater {
                                model: delegateRoot.rowStateIcons

                                Image {
                                    id: stateIcon
                                    source: modelData
                                    width: 18
                                    height: 18
                                    fillMode: Image.PreserveAspectFit

                                    HoverHandler {
                                        id: stateIconHover
                                    }
                                    ToolTip.text: delegateRoot.rowStateTip
                                    ToolTip.visible: stateIcon.ToolTip.text.length > 0 && stateIconHover.hovered
                                }
                            }
                        }
                        Text {
                            id: roomNameText
                            width: Math.max(0, delegateRoot.width - nameW - statW * root.statsVisibleCount - stateW)
                            height: rowH
                            leftPadding: 4
                            verticalAlignment: Text.AlignVCenter
                            elide: Text.ElideRight
                            text: model.roomName
                            color: model.textColor

                            HoverHandler {
                                id: roomNameHover
                            }
                            // Mirrors the widget's STATE-column tooltip (same
                            // Qt::ToolTipRole source) shown here on the room-name
                            // cell as well since GroupWidget's QTableView applies
                            // the row's tooltip role across the whole row.
                            ToolTip.text: model.stateTip ? model.stateTip : ""
                            ToolTip.visible: roomNameText.ToolTip.text.length > 0 && roomNameHover.hovered
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
    }

    Text {
        anchors.centerIn: parent
        visible: listView.count === 0
        text: qsTr("No group members")
        color: root.panelPalette.text
        opacity: 0.5
    }
}
