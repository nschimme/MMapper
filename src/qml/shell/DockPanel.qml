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

    // Id this panel is registered under in DockLayoutController (e.g.
    // "log") -- set by the caller (MainShell.qml) so the header's "move to"
    // menu below can call dockLayout.setDockArea(dockId, ...) without the
    // caller having to wire up 4 separate onTriggered handlers itself.
    // Purely a passthrough; this component never reads DockLayoutController
    // directly (mirrors closeRequested/floatRequested's own
    // caller-owns-the-property-write design -- see those signals' doc
    // comments below).
    property string dockId: ""

    // This panel's current dock area ("left"/"top"/"bottom"/"right"), set
    // by the caller from dockLayout.dockArea(dockId) -- used only to grey
    // out the "move to" menu's entry for the area the panel is already in.
    property string currentArea: ""

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

    // Emitted with "left"/"top"/"bottom"/"right" when the header's "move
    // to" menu picks a new area; the caller is expected to call
    // dockLayout.setDockArea(dockId, area) (see MainShell.qml).
    signal moveToAreaRequested(string area)

    implicitWidth: Math.max(header.implicitWidth,
                             loader.item ? loader.item.implicitWidth : 0)
    implicitHeight: header.implicitHeight + (loader.item ? loader.item.implicitHeight : 0)

    // Explicit preferred sizes so a SplitView ALWAYS allocates space to this
    // panel when it is added at runtime. MainShell.qml places docks
    // imperatively (SplitView.insertItem, see its "dock pool" comment); once
    // a SplitView has done its first sized layout, an item inserted later
    // with no preferred size gets ZERO space and renders invisible/
    // non-interactive (only the very first layout falls back to implicit
    // sizes -- which is why this only bites after the shell is already up and
    // a panel is moved). SplitView uses preferredWidth for a horizontal
    // parent (top/bottom rows) and preferredHeight for a vertical one
    // (left/right columns); setting both covers a panel in either kind of
    // area. Harmless when this DockPanel is a floating window's content
    // (anchors.fill governs there and the attached props are ignored).
    QQC2.SplitView.preferredWidth: 320
    QQC2.SplitView.preferredHeight: 220

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
            anchors.right: moveButton.left
            elide: Text.ElideRight
            text: root.title
            color: sysPalette.buttonText
        }

        // "Move to" menu: lets the user re-area this panel between the
        // shell's 4 fixed dock slots (left/top/bottom/right -- see
        // MainShell.qml's dockMeta/leftColumn-etc. comments) without
        // drag-and-drop, which QQC2.SplitView doesn't support here (see the
        // task report this shipped with). Placed left of floatButton so the
        // rightmost-to-leftmost header order stays [close][float][move],
        // matching floatButton/closeButton's existing right-to-left anchor
        // chain.
        QQC2.ToolButton {
            id: moveButton
            anchors.right: floatButton.left
            anchors.verticalCenter: parent.verticalCenter
            text: "⋮"
            QQC2.ToolTip.visible: hovered
            QQC2.ToolTip.text: qsTr("Move to…")
            implicitWidth: implicitHeight
            onClicked: moveMenu.popup()

            QQC2.Menu {
                id: moveMenu

                QQC2.MenuItem {
                    text: qsTr("Move to Left")
                    enabled: root.currentArea !== "left"
                    onTriggered: root.moveToAreaRequested("left")
                }
                QQC2.MenuItem {
                    text: qsTr("Move to Top")
                    enabled: root.currentArea !== "top"
                    onTriggered: root.moveToAreaRequested("top")
                }
                QQC2.MenuItem {
                    text: qsTr("Move to Bottom")
                    enabled: root.currentArea !== "bottom"
                    onTriggered: root.moveToAreaRequested("bottom")
                }
                QQC2.MenuItem {
                    text: qsTr("Move to Right")
                    enabled: root.currentArea !== "right"
                    onTriggered: root.moveToAreaRequested("right")
                }
            }
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
