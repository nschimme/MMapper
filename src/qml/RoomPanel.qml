import QtQuick
import QtQuick.Controls

import MMapper

PanelFrame {
    id: root

    // Context property expected to be set by C++ before this component is
    // instantiated: roomModel (RoomModel).

    // Modest "header + a couple of rows" minimum, mirroring the panel's
    // widget-era layout minimum: wider than the other list panels since
    // RoomModel has many columns (see RoomModel::ColumnTypeEnum) that would
    // otherwise get squashed unreadably narrow.
    implicitWidth: 320
    implicitHeight: 100

    color: root.panelPalette.base

    // Used to measure the pixel width of header/cell text so columns can be
    // sized to fit their widest content instead of collapsing to a fixed
    // default width.
    TextMetrics {
        id: tm
    }

    // Returns the pixel width the given column should be, based on the
    // widest of its header text and its content (see longestTextInColumn).
    // A minimum width and a small margin keep narrow columns readable.
    function columnWidth(col) {
        tm.text = roomModel.headerData(col, Qt.Horizontal);
        var w = tm.advanceWidth;
        tm.text = roomModel.longestTextInColumn(col);
        w = Math.max(w, tm.advanceWidth);
        return Math.max(w + 16, 48);
    }

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
            objectName: "roomTableView"
            width: parent.width
            height: parent.height - headerView.height
            clip: true
            model: roomModel
            columnSpacing: 0
            rowSpacing: 0
            columnWidthProvider: root.columnWidth

            ScrollBar.vertical: ScrollBar {}
            ScrollBar.horizontal: ScrollBar {}

            Connections {
                target: roomModel

                function onModelReset() {
                    Qt.callLater(tableView.forceLayout);
                }

                function onDataChanged() {
                    Qt.callLater(tableView.forceLayout);
                }
            }

            delegate: Rectangle {
                id: cellDelegate

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
