// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

import QtQuick
import QtQuick.Controls as QQC2

// A single side-panel dock inside MainShell.qml's SplitView layout
// (../shell/MainShell.qml, wrapped in DockColumn/DockRow -- see those
// files). Mirrors what QmlDockWidget (../QmlDockWidget.h/.cpp) gives the
// widget-shell docks -- a title bar with a close button, and the panel's
// own QML content below it -- except content here is loaded via `source`
// against MainShell.qml's single shared QQmlApplicationEngine rather than
// QmlDockWidget's own private per-dock QQuickWidget/engine, so the panel's
// context properties (e.g. `timerModel`) must already be on the shared
// engine's root context (see QmlShellWindow.cpp) before this loads.
//
// `visible` is intentionally left for the caller to bind against
// DockLayoutController (../DockLayoutController.h) -- e.g.
// `visible: dockLayout.logVisible` -- rather than owned here, so
// MainShell.qml's "Side Panels" menu and this panel's own close button stay
// in sync through the same single source of truth. The Loader itself stays
// active regardless of `visible` so a hidden panel's state (scroll
// position, filter text, ...) survives being toggled back on, matching
// QDockWidget::hide()/show() semantics.
Item {
    id: root

    // Title shown in the header bar.
    property string title: ""

    // URL of the panel's own QML file (e.g. "qrc:/qt/qml/MMapper/LogPanel.qml").
    property url source

    // True when this DockPanel is the content of a floating QtQuick.Window
    // (see MainShell.qml's FloatingDock inline component) rather than a
    // docked SplitView child. Purely cosmetic here -- it only flips the
    // float button's glyph/tooltip below -- the caller (MainShell.qml) is
    // the one actually deciding whether to dock or float a given panel via
    // DockLayoutController's xFloating property.
    property bool floating: false

    // Emitted when the header's close button is clicked; the caller is
    // expected to flip the DockLayoutController property this dock's
    // `visible` is bound to (see MainShell.qml).
    signal closeRequested

    // Emitted when the header's float/re-dock button is clicked; the caller
    // is expected to flip the DockLayoutController property this dock's
    // `floating` is bound to (see MainShell.qml).
    signal floatRequested

    implicitWidth: Math.max(header.implicitWidth,
                             loader.item ? loader.item.implicitWidth : 0)
    implicitHeight: header.implicitHeight + (loader.item ? loader.item.implicitHeight : 0)

    SystemPalette {
        id: sysPalette
        colorGroup: SystemPalette.Active
    }

    Rectangle {
        id: header
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        implicitHeight: titleText.implicitHeight + 8
        color: sysPalette.button

        Text {
            id: titleText
            anchors.left: parent.left
            anchors.leftMargin: 6
            anchors.verticalCenter: parent.verticalCenter
            anchors.right: floatButton.left
            elide: Text.ElideRight
            text: root.title
            color: sysPalette.buttonText
        }

        // Float/re-dock toggle, mirroring QDockWidget::DockWidgetFloatable's
        // title-bar button (see DockLayoutController.h's file comment).
        // "⧉" (docked -> click to float) and "⚓" (floating -> click to
        // re-dock) were picked over text labels to match closeButton's
        // glyph-only style and keep the header compact at the sidebar's
        // ~320px default width.
        QQC2.ToolButton {
            id: floatButton
            anchors.right: closeButton.left
            anchors.verticalCenter: parent.verticalCenter
            text: root.floating ? "⚓" : "⧉"
            QQC2.ToolTip.visible: hovered
            QQC2.ToolTip.text: root.floating ? qsTr("Dock") : qsTr("Float")
            implicitWidth: implicitHeight
            onClicked: root.floatRequested()
        }

        QQC2.ToolButton {
            id: closeButton
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            text: "✕"
            implicitWidth: implicitHeight
            onClicked: root.closeRequested()
        }
    }

    Loader {
        id: loader
        anchors.top: header.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        source: root.source
    }
}
