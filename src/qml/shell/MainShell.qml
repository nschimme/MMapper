// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

import QtQuick
import QtQuick.Controls as QQC2
import MMapper

// The first bootable top-level window of the QML shell (Shell B; see
// ../../main.cpp's --qml-shell handling and QmlShellWindow.h/.cpp, which
// constructs the QQmlApplicationEngine that loads this file). Mirrors
// MainWindow's full menu bar (setupMenuBar()), toolbars (setupToolBars())
// and status bar (setupStatusBar()) -- see ../../mainwindow/mainwindow.cpp
// -- wired to real CommandRegistry commands via CommandAction
// (../CommandAction.qml) wherever a live implementation exists; everything
// else stays a disabled placeholder (see QmlShellWindow.cpp's
// ALL_COMMAND_SPECS/isLiveCommand()). NOT feature-complete -- see
// QmlShellWindow.h's file comment for what's still missing relative to
// MainWindow.
//
// Context properties, set by QmlShellWindow before loading this file:
//   commands     -- CommandRegistry* (see ../../mainwindow/CommandRegistry.h)
//   mapCore      -- MapCanvasCore* (bound into MapView's `core` property)
//   mapViewModel -- MapViewModel*, consumed by MapView.qml's scrollbars
//   statusText   -- QString, shown in the footer's message label; funneled
//                   from a few real signals (see QmlShellWindow.cpp's
//                   "Funnels a few real signals" comment) -- full AppCore
//                   statusMessage integration is still a TODO there.
//   dockLayout   -- DockLayoutController* (../DockLayoutController.h),
//                   owns the 8 side-panel docks' visibility
//   toolbarLayout -- ToolbarLayoutController* (../ToolbarLayoutController.h),
//                   owns the 9 toolbars' visibility
//   mapZoom      -- MapZoomController* (../../display/MapZoomController.h),
//                   backs the View toolbar's zoom Slider
//   musicVolume, soundVolume -- AudioVolumeController*
//                   (../../mainwindow/AudioVolumeController.h), back the
//                   Audio toolbar's two volume Sliders
//   clock        -- ClockAdapter*, consumed by ClockStrip.qml in the footer
//   xpStatusAdapter -- XpStatusAdapter*, consumed by XpStatusItem.qml in the
//                   footer (named "xpStatusAdapter", not "adapter" --
//                   "adapter" is already the DescriptionPanel.qml context
//                   property on this shell's one shared root context; see
//                   QmlShellWindow.cpp's comment on the rename)
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
                action: CommandAction { cmd: commands ? commands.command("file.new") : null }
            }
            QQC2.MenuItem {
                action: CommandAction { cmd: commands ? commands.command("file.open") : null }
            }
            QQC2.MenuItem {
                action: CommandAction { cmd: commands ? commands.command("file.save") : null }
            }
            // Wasm hides Save As/Reload/Exit entirely (see mainwindow.cpp's
            // setupMenuBar() `if constexpr (CURRENT_PLATFORM != PlatformEnum::Wasm)`
            // guards); kept unconditional here for now -- shell B doesn't
            // ship on Wasm yet (see QmlShellWindow.h's file comment), so
            // there is no Wasm build of this file to gate. TODO(wasm shell
            // commit): mirror the platform guard when it does.
            QQC2.MenuItem {
                action: CommandAction { cmd: commands ? commands.command("file.save-as") : null }
            }
            QQC2.MenuItem {
                action: CommandAction { cmd: commands ? commands.command("file.reload") : null }
            }

            QQC2.MenuSeparator {}

            QQC2.Menu {
                id: fileExportMenu
                objectName: "fileExportMenu"
                title: qsTr("&Export")

                QQC2.MenuItem {
                    action: CommandAction {
                        cmd: commands ? commands.command("file.export.base-map") : null
                    }
                }
                QQC2.MenuItem {
                    action: CommandAction {
                        cmd: commands ? commands.command("file.export.mm2xml") : null
                    }
                }
                QQC2.MenuItem {
                    action: CommandAction {
                        cmd: commands ? commands.command("file.export.web") : null
                    }
                }
                QQC2.MenuItem {
                    action: CommandAction {
                        cmd: commands ? commands.command("file.export.mmp") : null
                    }
                }
            }

            QQC2.MenuItem {
                action: CommandAction { cmd: commands ? commands.command("file.merge") : null }
            }

            QQC2.MenuSeparator {}

            // TODO(wasm shell commit): see the Save As/Reload TODO above.
            QQC2.MenuItem {
                action: CommandAction { cmd: commands ? commands.command("file.exit") : null }
            }
        }

        QQC2.Menu {
            title: qsTr("&Edit")

            QQC2.Menu {
                title: qsTr("&Mode")

                QQC2.MenuItem {
                    action: CommandAction { cmd: commands ? commands.command("mapper-mode.play") : null }
                }
                QQC2.MenuItem {
                    action: CommandAction { cmd: commands ? commands.command("mapper-mode.map") : null }
                }
                QQC2.MenuItem {
                    action: CommandAction {
                        cmd: commands ? commands.command("mapper-mode.offline") : null
                    }
                }
            }

            QQC2.MenuSeparator {}

            QQC2.MenuItem {
                action: CommandAction { cmd: commands ? commands.command("edit.undo") : null }
            }
            QQC2.MenuItem {
                action: CommandAction { cmd: commands ? commands.command("edit.redo") : null }
            }

            QQC2.MenuSeparator {}

            QQC2.Menu {
                title: qsTr("M&arkers")

                QQC2.MenuItem {
                    action: CommandAction {
                        cmd: commands ? commands.command("mouse-mode.infomark-select") : null
                    }
                }
                QQC2.MenuSeparator {}
                QQC2.MenuItem {
                    action: CommandAction {
                        cmd: commands ? commands.command("mouse-mode.create-infomark") : null
                    }
                }
                QQC2.MenuItem {
                    action: CommandAction {
                        cmd: commands ? commands.command("infomark.edit-selected") : null
                    }
                }
                QQC2.MenuItem {
                    action: CommandAction {
                        cmd: commands ? commands.command("infomark.delete-selected") : null
                    }
                }
            }

            QQC2.Menu {
                id: editRoomsMenu
                objectName: "editRoomsMenu"
                title: qsTr("&Rooms")

                QQC2.MenuItem {
                    action: CommandAction {
                        cmd: commands ? commands.command("mouse-mode.room-select") : null
                    }
                }
                QQC2.MenuItem {
                    action: CommandAction {
                        cmd: commands ? commands.command("mouse-mode.room-raypick") : null
                    }
                }
                QQC2.MenuSeparator {}
                QQC2.MenuItem {
                    action: CommandAction {
                        cmd: commands ? commands.command("mouse-mode.create-room") : null
                    }
                }
                QQC2.MenuItem {
                    action: CommandAction { cmd: commands ? commands.command("room.edit-selected") : null }
                }
                QQC2.MenuItem {
                    action: CommandAction {
                        cmd: commands ? commands.command("room.delete-selected") : null
                    }
                }
                QQC2.MenuItem {
                    action: CommandAction {
                        cmd: commands ? commands.command("room.move-up-selected") : null
                    }
                }
                QQC2.MenuItem {
                    action: CommandAction {
                        cmd: commands ? commands.command("room.move-down-selected") : null
                    }
                }
                QQC2.MenuItem {
                    action: CommandAction {
                        cmd: commands ? commands.command("room.merge-up-selected") : null
                    }
                }
                QQC2.MenuItem {
                    action: CommandAction {
                        cmd: commands ? commands.command("room.merge-down-selected") : null
                    }
                }
                QQC2.MenuItem {
                    action: CommandAction {
                        cmd: commands ? commands.command("room.connect-to-neighbours") : null
                    }
                }
            }

            QQC2.Menu {
                title: qsTr("&Connections")

                QQC2.MenuItem {
                    action: CommandAction {
                        cmd: commands ? commands.command("mouse-mode.connection-select") : null
                    }
                }
                QQC2.MenuSeparator {}
                QQC2.MenuItem {
                    action: CommandAction {
                        cmd: commands ? commands.command("mouse-mode.create-connection") : null
                    }
                }
                QQC2.MenuItem {
                    action: CommandAction {
                        cmd: commands
                              ? commands.command("mouse-mode.create-oneway-connection") : null
                    }
                }
                QQC2.MenuItem {
                    action: CommandAction {
                        cmd: commands ? commands.command("connection.delete-selected") : null
                    }
                }
            }

            QQC2.MenuSeparator {}

            QQC2.MenuItem {
                action: CommandAction { cmd: commands ? commands.command("room.find") : null }
            }
            QQC2.MenuItem {
                action: CommandAction {
                    cmd: commands ? commands.command("edit.preferences") : null
                }
            }
        }

        QQC2.Menu {
            title: qsTr("&View")

            QQC2.MenuItem {
                action: CommandAction { cmd: commands ? commands.command("mouse-mode.move") : null }
            }

            // View -> Toolbars -> <toolbar>, mirroring mainwindow.cpp's
            // setupToolBars()/setupMenuBar() `toolbars->addAction(...
            // ->toggleViewAction())` calls. Backed by ToolbarLayoutController
            // (../ToolbarLayoutController.h) rather than a command: unlike
            // the "Side Panels" menu below, nothing else in this shell ever
            // mutates these properties (no per-toolbar close button), so a
            // plain `checked` binding is safe without the Connections
            // re-sync the dock items below need (see ToolbarLayoutController.h).
            QQC2.Menu {
                title: qsTr("&Toolbars")

                QQC2.MenuItem {
                    text: qsTr("File")
                    checkable: true
                    checked: toolbarLayout ? toolbarLayout.fileVisible : false
                    onTriggered: if (toolbarLayout) toolbarLayout.fileVisible = checked
                }
                QQC2.MenuItem {
                    text: qsTr("Mapper Mode")
                    checkable: true
                    checked: toolbarLayout ? toolbarLayout.mapperModeVisible : false
                    onTriggered: if (toolbarLayout) toolbarLayout.mapperModeVisible = checked
                }
                QQC2.MenuItem {
                    text: qsTr("Mouse Mode")
                    checkable: true
                    checked: toolbarLayout ? toolbarLayout.mouseModeVisible : false
                    onTriggered: if (toolbarLayout) toolbarLayout.mouseModeVisible = checked
                }
                QQC2.MenuItem {
                    text: qsTr("View")
                    checkable: true
                    checked: toolbarLayout ? toolbarLayout.viewVisible : false
                    onTriggered: if (toolbarLayout) toolbarLayout.viewVisible = checked
                }
                QQC2.MenuItem {
                    text: qsTr("Path Machine")
                    checkable: true
                    checked: toolbarLayout ? toolbarLayout.pathMachineVisible : false
                    onTriggered: if (toolbarLayout) toolbarLayout.pathMachineVisible = checked
                }
                QQC2.MenuItem {
                    text: qsTr("Rooms")
                    checkable: true
                    checked: toolbarLayout ? toolbarLayout.roomsVisible : false
                    onTriggered: if (toolbarLayout) toolbarLayout.roomsVisible = checked
                }
                QQC2.MenuItem {
                    text: qsTr("Connections")
                    checkable: true
                    checked: toolbarLayout ? toolbarLayout.connectionsVisible : false
                    onTriggered: if (toolbarLayout) toolbarLayout.connectionsVisible = checked
                }
                QQC2.MenuItem {
                    text: qsTr("Preferences")
                    checkable: true
                    checked: toolbarLayout ? toolbarLayout.preferencesVisible : false
                    onTriggered: if (toolbarLayout) toolbarLayout.preferencesVisible = checked
                }
                QQC2.MenuItem {
                    text: qsTr("Audio")
                    checkable: true
                    checked: toolbarLayout ? toolbarLayout.audioVisible : false
                    onTriggered: if (toolbarLayout) toolbarLayout.audioVisible = checked
                }
            }

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

            QQC2.MenuItem {
                action: CommandAction {
                    cmd: commands ? commands.command("world.rebuild-meshes") : null
                }
            }

            QQC2.MenuSeparator {}

            QQC2.MenuItem {
                action: CommandAction {
                    cmd: commands ? commands.command("view.show-status-bar") : null
                }
            }
            QQC2.MenuItem {
                action: CommandAction {
                    cmd: commands ? commands.command("view.show-scroll-bars") : null
                }
            }
            QQC2.MenuItem {
                action: CommandAction {
                    cmd: commands ? commands.command("view.show-menu-bar") : null
                }
            }
            QQC2.MenuItem {
                action: CommandAction {
                    cmd: commands ? commands.command("view.always-on-top") : null
                }
            }
        }

        QQC2.Menu {
            title: qsTr("&Tools")

            QQC2.Menu {
                title: qsTr("&Integrated Mud Client")

                QQC2.MenuItem {
                    action: CommandAction { cmd: commands ? commands.command("client.launch") : null }
                }
                QQC2.MenuItem {
                    action: CommandAction {
                        cmd: commands ? commands.command("client.save-log") : null
                    }
                }
                QQC2.MenuItem {
                    action: CommandAction {
                        cmd: commands ? commands.command("client.save-log-html") : null
                    }
                }
            }

            QQC2.Menu {
                title: qsTr("&Path Machine")

                QQC2.MenuItem {
                    action: CommandAction {
                        cmd: commands ? commands.command("mouse-mode.room-select") : null
                    }
                }
                QQC2.MenuSeparator {}
                QQC2.MenuItem {
                    action: CommandAction {
                        cmd: commands ? commands.command("room.goto-selected") : null
                    }
                }
                QQC2.MenuItem {
                    action: CommandAction {
                        cmd: commands ? commands.command("room.force-update-selected") : null
                    }
                }
                QQC2.MenuItem {
                    action: CommandAction {
                        cmd: commands ? commands.command("pathmachine.release-all-paths") : null
                    }
                }
            }
        }

        QQC2.Menu {
            title: qsTr("&Help")

            QQC2.MenuItem {
                action: CommandAction { cmd: commands ? commands.command("help.setup") : null }
            }
            QQC2.MenuItem {
                action: CommandAction {
                    cmd: commands ? commands.command("help.report-issue") : null
                }
            }
            QQC2.MenuItem {
                action: CommandAction {
                    cmd: commands ? commands.command("help.check-for-update") : null
                }
            }

            QQC2.MenuSeparator {}

            QQC2.Menu {
                title: qsTr("M&UME")

                QQC2.MenuItem {
                    action: CommandAction { cmd: commands ? commands.command("help.vote") : null }
                }
                QQC2.MenuItem {
                    action: CommandAction { cmd: commands ? commands.command("help.newbie") : null }
                }
                QQC2.MenuItem {
                    action: CommandAction { cmd: commands ? commands.command("help.website") : null }
                }
                QQC2.MenuItem {
                    action: CommandAction { cmd: commands ? commands.command("help.forum") : null }
                }
                QQC2.MenuItem {
                    action: CommandAction { cmd: commands ? commands.command("help.wiki") : null }
                }
            }

            QQC2.MenuSeparator {}

            QQC2.MenuItem {
                action: CommandAction { cmd: commands ? commands.command("help.about") : null }
            }
            // TODO(wasm shell commit): see the File menu's Save As/Reload TODO.
            QQC2.MenuItem {
                action: CommandAction { cmd: commands ? commands.command("help.about-qt") : null }
            }
        }
    }

    // 9 toolbars, mirroring MainWindow::setupToolBars() one-for-one (same
    // order, same contents); each is a plain QQC2.ToolBar row stacked in
    // this header Column, visible only when its ToolbarLayoutController
    // property is true -- all 9 default to false, matching every toolbar's
    // unconditional ->hide() call at the end of setupToolBars(). Unlike
    // QToolBar, a QQC2.ToolBar can't float or be dragged to another edge;
    // that's out of scope for this shell (see DockLayoutController.h's file
    // comment for the same tradeoff on docks).
    header: Column {
        QQC2.ToolBar {
            objectName: "fileToolBar"
            visible: toolbarLayout ? toolbarLayout.fileVisible : false
            width: parent.width
            Row {
                anchors.verticalCenter: parent.verticalCenter
                spacing: 4
                QQC2.ToolButton {
                    action: CommandAction { cmd: commands ? commands.command("file.new") : null }
                }
                QQC2.ToolButton {
                    action: CommandAction { cmd: commands ? commands.command("file.open") : null }
                }
                QQC2.ToolButton {
                    action: CommandAction { cmd: commands ? commands.command("file.save") : null }
                }
            }
        }

        QQC2.ToolBar {
            objectName: "mapperModeToolBar"
            visible: toolbarLayout ? toolbarLayout.mapperModeVisible : false
            width: parent.width
            Row {
                anchors.verticalCenter: parent.verticalCenter
                spacing: 4
                QQC2.ToolButton {
                    action: CommandAction { cmd: commands ? commands.command("mapper-mode.play") : null }
                }
                QQC2.ToolButton {
                    action: CommandAction { cmd: commands ? commands.command("mapper-mode.map") : null }
                }
                QQC2.ToolButton {
                    action: CommandAction {
                        cmd: commands ? commands.command("mapper-mode.offline") : null
                    }
                }
            }
        }

        QQC2.ToolBar {
            objectName: "mouseModeToolBar"
            visible: toolbarLayout ? toolbarLayout.mouseModeVisible : false
            width: parent.width
            Row {
                anchors.verticalCenter: parent.verticalCenter
                spacing: 4
                QQC2.ToolButton {
                    action: CommandAction { cmd: commands ? commands.command("mouse-mode.move") : null }
                }
                QQC2.ToolButton {
                    action: CommandAction {
                        cmd: commands ? commands.command("mouse-mode.room-raypick") : null
                    }
                }
                QQC2.ToolButton {
                    action: CommandAction {
                        cmd: commands ? commands.command("mouse-mode.room-select") : null
                    }
                }
                QQC2.ToolButton {
                    action: CommandAction {
                        cmd: commands ? commands.command("mouse-mode.connection-select") : null
                    }
                }
                QQC2.ToolButton {
                    action: CommandAction {
                        cmd: commands ? commands.command("mouse-mode.create-room") : null
                    }
                }
                QQC2.ToolButton {
                    action: CommandAction {
                        cmd: commands ? commands.command("mouse-mode.create-connection") : null
                    }
                }
                QQC2.ToolButton {
                    action: CommandAction {
                        cmd: commands
                              ? commands.command("mouse-mode.create-oneway-connection") : null
                    }
                }
                QQC2.ToolButton {
                    action: CommandAction {
                        cmd: commands ? commands.command("mouse-mode.infomark-select") : null
                    }
                }
                QQC2.ToolButton {
                    action: CommandAction {
                        cmd: commands ? commands.command("mouse-mode.create-infomark") : null
                    }
                }
            }
        }

        QQC2.ToolBar {
            objectName: "viewToolBar"
            visible: toolbarLayout ? toolbarLayout.viewVisible : false
            width: parent.width
            Row {
                anchors.verticalCenter: parent.verticalCenter
                spacing: 4
                QQC2.ToolButton {
                    action: CommandAction { cmd: commands ? commands.command("view.zoom-in") : null }
                }
                QQC2.ToolButton {
                    action: CommandAction { cmd: commands ? commands.command("view.zoom-out") : null }
                }
                QQC2.ToolButton {
                    action: CommandAction {
                        cmd: commands ? commands.command("view.zoom-reset") : null
                    }
                }
                // Mirrors MapZoomSlider's log-scale position<->zoom mapping
                // (see ../../display/MapZoomController.h); `value` binds
                // one-way from the controller and `onMoved` (not
                // onValueChanged, which would also fire from the
                // programmatic update below) writes user drags back,
                // avoiding the feedback loop MapZoomSlider's SignalBlocker
                // guards against on the widget side.
                QQC2.Slider {
                    id: zoomSlider
                    objectName: "zoomSlider"
                    width: 120
                    from: mapZoom ? mapZoom.minimum : 0
                    to: mapZoom ? mapZoom.maximum : 0
                    value: mapZoom ? mapZoom.position : 0
                    onMoved: if (mapZoom) mapZoom.position = Math.round(value)
                }
                QQC2.ToolButton {
                    action: CommandAction { cmd: commands ? commands.command("layer.up") : null }
                }
                QQC2.ToolButton {
                    action: CommandAction { cmd: commands ? commands.command("layer.down") : null }
                }
                QQC2.ToolButton {
                    action: CommandAction { cmd: commands ? commands.command("layer.reset") : null }
                }
            }
        }

        QQC2.ToolBar {
            objectName: "pathMachineToolBar"
            visible: toolbarLayout ? toolbarLayout.pathMachineVisible : false
            width: parent.width
            Row {
                anchors.verticalCenter: parent.verticalCenter
                spacing: 4
                QQC2.ToolButton {
                    action: CommandAction {
                        cmd: commands ? commands.command("pathmachine.release-all-paths") : null
                    }
                }
                QQC2.ToolButton {
                    action: CommandAction {
                        cmd: commands ? commands.command("room.goto-selected") : null
                    }
                }
                QQC2.ToolButton {
                    action: CommandAction {
                        cmd: commands ? commands.command("room.force-update-selected") : null
                    }
                }
            }
        }

        QQC2.ToolBar {
            objectName: "roomToolBar"
            visible: toolbarLayout ? toolbarLayout.roomsVisible : false
            width: parent.width
            Row {
                anchors.verticalCenter: parent.verticalCenter
                spacing: 4
                QQC2.ToolButton {
                    action: CommandAction { cmd: commands ? commands.command("room.find") : null }
                }
                QQC2.ToolButton {
                    action: CommandAction { cmd: commands ? commands.command("room.edit-selected") : null }
                }
                QQC2.ToolButton {
                    action: CommandAction {
                        cmd: commands ? commands.command("room.delete-selected") : null
                    }
                }
                QQC2.ToolButton {
                    action: CommandAction {
                        cmd: commands ? commands.command("room.move-up-selected") : null
                    }
                }
                QQC2.ToolButton {
                    action: CommandAction {
                        cmd: commands ? commands.command("room.move-down-selected") : null
                    }
                }
                QQC2.ToolButton {
                    action: CommandAction {
                        cmd: commands ? commands.command("room.merge-up-selected") : null
                    }
                }
                QQC2.ToolButton {
                    action: CommandAction {
                        cmd: commands ? commands.command("room.merge-down-selected") : null
                    }
                }
                QQC2.ToolButton {
                    action: CommandAction {
                        cmd: commands ? commands.command("room.connect-to-neighbours") : null
                    }
                }
            }
        }

        QQC2.ToolBar {
            objectName: "connectionToolBar"
            visible: toolbarLayout ? toolbarLayout.connectionsVisible : false
            width: parent.width
            Row {
                anchors.verticalCenter: parent.verticalCenter
                spacing: 4
                QQC2.ToolButton {
                    action: CommandAction {
                        cmd: commands ? commands.command("connection.delete-selected") : null
                    }
                }
            }
        }

        QQC2.ToolBar {
            objectName: "settingsToolBar"
            visible: toolbarLayout ? toolbarLayout.preferencesVisible : false
            width: parent.width
            Row {
                anchors.verticalCenter: parent.verticalCenter
                spacing: 4
                QQC2.ToolButton {
                    action: CommandAction {
                        cmd: commands ? commands.command("edit.preferences") : null
                    }
                }
            }
        }

        QQC2.ToolBar {
            objectName: "audioToolBar"
            visible: toolbarLayout ? toolbarLayout.audioVisible : false
            width: parent.width
            Row {
                anchors.verticalCenter: parent.verticalCenter
                spacing: 4
                QQC2.Slider {
                    id: musicVolumeSlider
                    objectName: "musicVolumeSlider"
                    width: 100
                    from: 0
                    to: 100
                    stepSize: 1
                    value: musicVolume ? musicVolume.volume : 0
                    onMoved: if (musicVolume) musicVolume.volume = Math.round(value)
                }
                QQC2.ToolSeparator {}
                QQC2.Slider {
                    id: soundVolumeSlider
                    objectName: "soundVolumeSlider"
                    width: 100
                    from: 0
                    to: 100
                    stepSize: 1
                    value: soundVolume ? soundVolume.volume : 0
                    onMoved: if (soundVolume) soundVolume.volume = Math.round(value)
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

    // Footer status bar row, mirroring MainWindow::setupStatusBar():
    // ClockStrip.qml/XpStatusItem.qml permanent widgets (inserted at index
    // 0, i.e. leftmost of the permanent widgets, which visually places them
    // at the status bar's right edge -- see QStatusBar::insertPermanentWidget()'s
    // docs), a path-machine QLabel placeholder (no PathMachine wired into
    // this offline shell yet -- see the TODO below), and the transient
    // message label filling the remaining space on the left, exactly like
    // QStatusBar::showMessage()'s text does relative to permanent widgets.
    footer: Row {
        QQC2.Label {
            objectName: "statusLabel"
            text: typeof statusText !== "undefined" ? statusText : ""
            padding: 4
            width: parent.width - pathMachineLabel.implicitWidth - clockStrip.implicitWidth
                   - xpStatusItem.implicitWidth - 12
        }
        QQC2.Label {
            id: pathMachineLabel
            objectName: "pathMachineLabel"
            // TODO(shell commit): wire to Mmapper2PathMachine::sig_state
            // once a PathMachine is constructed in this shell (see
            // QmlShellWindow.h's file comment: "Deliberately NOT
            // constructed here" -- the async task engine/path machine isn't
            // part of this offline bring-up yet).
            text: ""
            padding: 4
        }
        XpStatusItem {
            id: xpStatusItem
            objectName: "xpStatusItem"
        }
        ClockStrip {
            id: clockStrip
            objectName: "clockStrip"
        }
    }
}
