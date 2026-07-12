// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

import QtQuick

// Shared table-header strip matching the Fusion HorizontalHeaderView look
// used by RoomPanel, for panels whose body is a ListView rather than a
// TableView (HorizontalHeaderView requires a syncView TableView).
//
// Styling mirrors QtQuick.Controls.Fusion's HorizontalHeaderView delegate
// (.../Fusion/HorizontalHeaderView.qml): 8px cell padding, a top-to-bottom
// gradient, a 1px border around every cell, and centered text. The upstream
// delegate hardcodes its gradient/border/text colors (#fbfbfb -> #e0dfe0,
// #cacaca, #ff26282a) regardless of palette, which looks wrong in dark
// theme; this component swaps those literals for the closest SystemPalette
// roles (light -> button, mid, text) so the header tracks light/dark theme
// switches instead.
Row {
    id: root

    // List of {text: string, width: real} objects describing each header
    // cell, left to right. A column may set fillWidth: true instead of a
    // fixed width to stretch and consume the row's remaining width; at most
    // one column (conventionally the last) should do this. Consumers must
    // bind an explicit "width" (e.g. "width: parent.width") for fillWidth
    // to have anything to distribute.
    property var columns: []

    readonly property real cellPadding: 8

    // Matches the Fusion delegate's per-cell implicitHeight formula
    // (Math.max(control.height, text.implicitHeight + cellPadding * 2))
    // minus the control.height term, which has no equivalent here since
    // this Row has no syncView TableView to size against. Row's own
    // implicitHeight is read-only (it is derived from its tallest child),
    // so this feeds each cell's explicit height below instead; giving every
    // cell that same height makes Row's implicitHeight equal to it.
    readonly property real headerHeight: headerMetrics.height + cellPadding * 2

    SystemPalette {
        id: sysPalette
        colorGroup: SystemPalette.Active
    }

    // Used only to measure a representative line height for implicitHeight;
    // not tied to any particular column's text.
    TextMetrics {
        id: headerMetrics
        text: "Xg"
    }

    function fixedWidthSum() {
        var sum = 0;
        for (var i = 0; i < columns.length; ++i) {
            if (!columns[i].fillWidth) {
                sum += columns[i].width;
            }
        }
        return sum;
    }

    Repeater {
        model: root.columns

        Rectangle {
            id: cell
            width: modelData.fillWidth === true ? Math.max(0, root.width - root.fixedWidthSum())
                                                 : modelData.width
            height: root.headerHeight
            border.color: sysPalette.mid

            gradient: Gradient {
                GradientStop {
                    position: 0
                    color: sysPalette.light
                }
                GradientStop {
                    position: 1
                    color: sysPalette.button
                }
            }

            Text {
                anchors.fill: parent
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
                text: modelData.text
                color: sysPalette.text
            }
        }
    }
}
