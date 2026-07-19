// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

import QtQuick.Controls as QQC2

// A vertically-stacked column of DockPanels (../shell/DockPanel.qml), used
// for MainShell.qml's left/right sidebars. Thin naming wrapper around
// QQC2.SplitView: SplitView's default property is already `data`, so
// DockPanel children declared inside a DockColumn become its arranged
// views with no extra plumbing needed here.
QQC2.SplitView {
    orientation: Qt.Vertical
}
