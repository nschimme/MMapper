// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

import QtQuick
import QtQuick.Controls as QQC2
import MMapper

// The first bootable top-level window of the QML shell (Shell B; see
// ../../main.cpp's --qml-shell handling and QmlShellWindow.h/.cpp, which
// constructs the QQmlApplicationEngine that loads this file). A pragmatic,
// minimal bring-up: a subset of MainWindow's menus (see
// ../../mainwindow/mainwindow.cpp's createActions()/menu building) wired
// to real CommandRegistry commands via CommandAction (../CommandAction.qml),
// the map itself (MapView.qml, unchanged from its own commit), and a
// static status label. NOT feature-complete -- see QmlShellWindow.h's file
// comment for what's still missing relative to MainWindow.
//
// Context properties, set by QmlShellWindow before loading this file:
//   commands     -- CommandRegistry* (see ../../mainwindow/CommandRegistry.h)
//   mapCore      -- MapCanvasCore* (bound into MapView's `core` property)
//   mapViewModel -- MapViewModel*, consumed by MapView.qml's scrollbars
//   statusText   -- QString, shown in the footer (currently static -- see
//                   QmlShellWindow.cpp's TODO on wiring up AppCore)
//   dockLayout   -- DockLayoutController* (../DockLayoutController.h),
//                   owns the 8 side-panel docks' visibility
//   logModel, groupModel/groupProxyModel/groupController, roomModel,
//   adventureLogModel, adapter, timerModel/timerController, tasksModel,
//   clientController/clientLineModel -- one set of context properties per
//   panel, matching what mainwindow.cpp's QmlDockWidget::setContextProperty()
//   calls set on each dock's own private engine; here they all live on this
//   single shared engine's root context instead (see QmlShellWindow.cpp).
//   config -- QmlConfig*, shared by GroupPanel.qml and ClientPanel.qml
//   (same object mainwindow.cpp passes to both).
QQC2.ApplicationWindow {
    id: window

    width: 1280
    height: 800
    visible: true
    title: qsTr("MMapper (QML shell preview)")

    menuBar: QQC2.MenuBar {
        QQC2.Menu {
            title: qsTr("&File")

            QQC2.MenuItem {
                action: CommandAction { cmd: commands ? commands.command("file.exit") : null }
            }
        }

        QQC2.Menu {
            title: qsTr("&View")

            QQC2.MenuItem {
                action: CommandAction { cmd: commands ? commands.command("view.zoom-in") : null }
            }
            QQC2.MenuItem {
                action: CommandAction { cmd: commands ? commands.command("view.zoom-out") : null }
            }
            QQC2.MenuItem {
                action: CommandAction { cmd: commands ? commands.command("view.zoom-reset") : null }
            }

            QQC2.MenuSeparator {}

            QQC2.MenuItem {
                action: CommandAction { cmd: commands ? commands.command("layer.up") : null }
            }
            QQC2.MenuItem {
                action: CommandAction { cmd: commands ? commands.command("layer.down") : null }
            }
            QQC2.MenuItem {
                action: CommandAction { cmd: commands ? commands.command("layer.reset") : null }
            }

            QQC2.MenuSeparator {}

            // View -> Side Panels -> <dock>, mirroring mainwindow.cpp's
            // dock construction comments of the same name. Each item binds
            // straight to a DockLayoutController property (see
            // ../DockLayoutController.h); the Connections block re-syncs
            // `checked` after every click because QQC2.MenuItem's built-in
            // checkable-click handling overwrites `checked` imperatively,
            // which (per normal QML semantics) permanently breaks a plain
            // `checked: dockLayout.xVisible` binding expression the first
            // time the item is clicked -- without the explicit re-sync, a
            // dock hidden via its own close button (DockPanel.qml) would
            // stop updating this menu item's checkmark after that.
            QQC2.Menu {
                title: qsTr("Side &Panels")

                QQC2.MenuItem {
                    id: logPanelMenuItem
                    // MenuItem has no "shortcut" property of its own (only
                    // Action does); mirrors mainwindow.cpp's
                    // dock->toggleViewAction()->setShortcut(tr("Ctrl+L")).
                    action: QQC2.Action {
                        text: qsTr("Log Panel")
                        checkable: true
                        checked: dockLayout ? dockLayout.logVisible : false
                        shortcut: "Ctrl+L"
                        onTriggered: if (dockLayout) dockLayout.logVisible = checked
                    }
                    Connections {
                        target: dockLayout
                        function onLogVisibleChanged() {
                            logPanelMenuItem.action.checked = dockLayout.logVisible;
                        }
                    }
                }
                QQC2.MenuItem {
                    id: groupPanelMenuItem
                    text: qsTr("Group Panel")
                    checkable: true
                    checked: dockLayout ? dockLayout.groupVisible : false
                    onTriggered: if (dockLayout) dockLayout.groupVisible = checked
                    Connections {
                        target: dockLayout
                        function onGroupVisibleChanged() {
                            groupPanelMenuItem.checked = dockLayout.groupVisible;
                        }
                    }
                }
                QQC2.MenuItem {
                    id: roomPanelMenuItem
                    text: qsTr("Room Panel")
                    checkable: true
                    checked: dockLayout ? dockLayout.roomVisible : false
                    onTriggered: if (dockLayout) dockLayout.roomVisible = checked
                    Connections {
                        target: dockLayout
                        function onRoomVisibleChanged() {
                            roomPanelMenuItem.checked = dockLayout.roomVisible;
                        }
                    }
                }
                QQC2.MenuItem {
                    id: adventurePanelMenuItem
                    text: qsTr("Adventure Panel")
                    checkable: true
                    checked: dockLayout ? dockLayout.adventureVisible : false
                    onTriggered: if (dockLayout) dockLayout.adventureVisible = checked
                    Connections {
                        target: dockLayout
                        function onAdventureVisibleChanged() {
                            adventurePanelMenuItem.checked = dockLayout.adventureVisible;
                        }
                    }
                }
                QQC2.MenuItem {
                    id: descriptionPanelMenuItem
                    text: qsTr("Description Panel")
                    checkable: true
                    checked: dockLayout ? dockLayout.descriptionVisible : false
                    onTriggered: if (dockLayout) dockLayout.descriptionVisible = checked
                    Connections {
                        target: dockLayout
                        function onDescriptionVisibleChanged() {
                            descriptionPanelMenuItem.checked = dockLayout.descriptionVisible;
                        }
                    }
                }
                QQC2.MenuItem {
                    id: timersPanelMenuItem
                    text: qsTr("Timers Panel")
                    checkable: true
                    checked: dockLayout ? dockLayout.timersVisible : false
                    onTriggered: if (dockLayout) dockLayout.timersVisible = checked
                    Connections {
                        target: dockLayout
                        function onTimersVisibleChanged() {
                            timersPanelMenuItem.checked = dockLayout.timersVisible;
                        }
                    }
                }
                QQC2.MenuItem {
                    id: tasksPanelMenuItem
                    text: qsTr("Tasks Panel")
                    checkable: true
                    checked: dockLayout ? dockLayout.tasksVisible : false
                    onTriggered: if (dockLayout) dockLayout.tasksVisible = checked
                    Connections {
                        target: dockLayout
                        function onTasksVisibleChanged() {
                            tasksPanelMenuItem.checked = dockLayout.tasksVisible;
                        }
                    }
                }
                QQC2.MenuItem {
                    id: clientPanelMenuItem
                    text: qsTr("Client Panel")
                    checkable: true
                    checked: dockLayout ? dockLayout.clientVisible : false
                    onTriggered: if (dockLayout) dockLayout.clientVisible = checked
                    Connections {
                        target: dockLayout
                        function onClientVisibleChanged() {
                            clientPanelMenuItem.checked = dockLayout.clientVisible;
                        }
                    }
                }
            }

            QQC2.MenuSeparator {}

            QQC2.MenuItem {
                action: CommandAction {
                    cmd: commands ? commands.command("world.rebuild-meshes") : null
                }
            }
        }

        QQC2.Menu {
            title: qsTr("&Mode")

            // Mirrors MainWindow::createActions()'s exclusive "mouse-mode"
            // QActionGroup (mainwindow.cpp): CommandRegistry enforces the
            // same exclusivity (see addToGroup()/enforceExclusive() in
            // CommandRegistry.cpp), so checking one of these unchecks the
            // rest without any QML-side bookkeeping.
            QQC2.MenuItem {
                action: CommandAction { cmd: commands ? commands.command("mouse-mode.move") : null }
            }
            QQC2.MenuItem {
                action: CommandAction {
                    cmd: commands ? commands.command("mouse-mode.room-raypick") : null
                }
            }
            QQC2.MenuItem {
                action: CommandAction {
                    cmd: commands ? commands.command("mouse-mode.room-select") : null
                }
            }
            QQC2.MenuItem {
                action: CommandAction {
                    cmd: commands ? commands.command("mouse-mode.connection-select") : null
                }
            }
            QQC2.MenuItem {
                action: CommandAction {
                    cmd: commands ? commands.command("mouse-mode.create-room") : null
                }
            }
            QQC2.MenuItem {
                action: CommandAction {
                    cmd: commands ? commands.command("mouse-mode.create-connection") : null
                }
            }
            QQC2.MenuItem {
                action: CommandAction {
                    cmd: commands ? commands.command("mouse-mode.create-oneway-connection") : null
                }
            }
            QQC2.MenuItem {
                action: CommandAction {
                    cmd: commands ? commands.command("mouse-mode.infomark-select") : null
                }
            }
            QQC2.MenuItem {
                action: CommandAction {
                    cmd: commands ? commands.command("mouse-mode.create-infomark") : null
                }
            }
        }
    }

    // Nested SplitView dock layout: horizontal [left column | vertical
    // [top row | map | bottom row] | right column]. Areas/default
    // visibility mirror mainwindow.cpp's addDockWidget() calls exactly
    // (see the "View -> Side Panels -> <dock>" comments there): Group
    // top/visible, Log/Room/Adventure/Tasks bottom/hidden, Description
    // right/visible, Timers right/hidden, Client left/visible.
    //
    // Floating docks (QDockWidget::DockWidgetFloatable on the widget side)
    // are not implemented here -- see DockLayoutController.h's file
    // comment -- every dock lives in one of these four fixed slots.
    QQC2.SplitView {
        id: outerSplit
        anchors.fill: parent
        orientation: Qt.Horizontal

        DockColumn {
            id: leftColumn
            QQC2.SplitView.preferredWidth: 320

            DockPanel {
                objectName: "dockClient"
                title: qsTr("Client Panel")
                source: "qrc:/qt/qml/MMapper/ClientPanel.qml"
                visible: dockLayout ? dockLayout.clientVisible : true
                onCloseRequested: if (dockLayout) dockLayout.clientVisible = false
            }
        }

        QQC2.SplitView {
            id: middleSplit
            orientation: Qt.Vertical
            QQC2.SplitView.fillWidth: true

            DockRow {
                id: topRow
                QQC2.SplitView.preferredHeight: 220

                DockPanel {
                    objectName: "dockGroup"
                    title: qsTr("Group Panel")
                    source: "qrc:/qt/qml/MMapper/GroupPanel.qml"
                    visible: dockLayout ? dockLayout.groupVisible : true
                    onCloseRequested: if (dockLayout) dockLayout.groupVisible = false
                }
            }

            MapView {
                id: mapView
                QQC2.SplitView.fillHeight: true
                core: typeof mapCore !== "undefined" ? mapCore : null
            }

            DockRow {
                id: bottomRow
                QQC2.SplitView.preferredHeight: 220

                DockPanel {
                    objectName: "dockLog"
                    title: qsTr("Log Panel")
                    source: "qrc:/qt/qml/MMapper/LogPanel.qml"
                    visible: dockLayout ? dockLayout.logVisible : false
                    onCloseRequested: if (dockLayout) dockLayout.logVisible = false
                }
                DockPanel {
                    objectName: "dockRoom"
                    title: qsTr("Room Panel")
                    source: "qrc:/qt/qml/MMapper/RoomPanel.qml"
                    visible: dockLayout ? dockLayout.roomVisible : false
                    onCloseRequested: if (dockLayout) dockLayout.roomVisible = false
                }
                DockPanel {
                    objectName: "dockAdventure"
                    title: qsTr("Adventure Panel")
                    source: "qrc:/qt/qml/MMapper/AdventurePanel.qml"
                    visible: dockLayout ? dockLayout.adventureVisible : false
                    onCloseRequested: if (dockLayout) dockLayout.adventureVisible = false
                }
                DockPanel {
                    objectName: "dockTasks"
                    title: qsTr("Tasks Panel")
                    source: "qrc:/qt/qml/MMapper/TasksPanel.qml"
                    visible: dockLayout ? dockLayout.tasksVisible : false
                    onCloseRequested: if (dockLayout) dockLayout.tasksVisible = false
                }
            }
        }

        DockColumn {
            id: rightColumn
            QQC2.SplitView.preferredWidth: 320

            DockPanel {
                objectName: "dockDescription"
                title: qsTr("Description Panel")
                source: "qrc:/qt/qml/MMapper/DescriptionPanel.qml"
                visible: dockLayout ? dockLayout.descriptionVisible : true
                onCloseRequested: if (dockLayout) dockLayout.descriptionVisible = false
            }
            DockPanel {
                objectName: "dockTimers"
                title: qsTr("Timers Panel")
                source: "qrc:/qt/qml/MMapper/TimerPanel.qml"
                visible: dockLayout ? dockLayout.timersVisible : false
                onCloseRequested: if (dockLayout) dockLayout.timersVisible = false
            }
        }
    }

    footer: QQC2.Label {
        objectName: "statusLabel"
        text: typeof statusText !== "undefined" ? statusText : ""
        padding: 4
    }
}
