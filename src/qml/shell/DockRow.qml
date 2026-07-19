// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

import QtQuick.Controls as QQC2

// A horizontally-arranged row of DockPanels (../shell/DockPanel.qml), used
// for MainShell.qml's top/bottom strips. See DockColumn.qml's comment --
// same reasoning, just Qt.Horizontal.
QQC2.SplitView {
    orientation: Qt.Horizontal
}
