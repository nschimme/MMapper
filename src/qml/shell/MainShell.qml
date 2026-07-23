// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Window
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
//   pathMachineStatus -- QString, shown in the footer's pathMachineLabel;
//                   fed by Mmapper2PathMachine::sig_state (see
//                   QmlShellWindow.cpp's wirePathMachine()).
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
//   windowTitle -- QString, this window's title (see
//                  QmlShellWindow::updateWindowTitle()).
//   mapLoaded   -- bool, hides MapView.qml's splash overlay once true (see
//                  QmlShellWindow::hideSplash()); never goes back to false.
//   ioTask      -- IoTaskController* (../../mainwindow/IoTaskController.h),
//                  drives ioProgressPopup below with the currently running
//                  load/merge/save task's label/percent/cancelability.
//   shell       -- QmlShellWindow* itself, exposing confirmClose() (see
//                  onClosing below) to QML.
QQC2.ApplicationWindow {
    id: window

    width: 1280
    height: 800
    visible: true

    // Auto-hide "peek" reveal for the menu bar and status bar: when their
    // "Always Show ..." toggle (view.show-menu-bar / view.show-status-bar) is
    // off, moving the pointer to the very top / bottom edge of the window
    // slides the bar back in (like a Windows auto-hidden taskbar), and it
    // hides again shortly after the pointer leaves it. `...Shown` mirrors the
    // persisted toggle; `...Peek` is the transient hover reveal; the bar is
    // visible when either is true.
    readonly property bool menuBarShown: commands && commands.command("view.show-menu-bar")
                                         ? commands.command("view.show-menu-bar").checked : true
    readonly property bool statusBarShown: commands && commands.command("view.show-status-bar")
                                           ? commands.command("view.show-status-bar").checked : true
    property bool menuBarPeek: false
    property bool statusBarPeek: false

    // Scope 13a: reactive compact/mobile breakpoint. This is a plain live
    // binding on `width`, so a window resize or a phone/tablet rotation
    // re-evaluates it (and everything bound to it below) instantly -- no
    // reload, no explicit orientation handling needed. Desktop behavior is
    // exactly `compact === false`; every compact-only bit of markup below is
    // additive and gated on this flag, so the desktop code path is
    // unchanged when it's false.
    readonly property bool compact: width < 720

    // Ids of the docks currently docked-and-shown in ANY of the 4 areas, in
    // area order (left, top, bottom, right) -- used only by the compact
    // drawer below to populate its tab list. Reuses areaIds() (further down)
    // rather than duplicating DockLayoutController's area lists; recomputes
    // whenever any of the 4 *DockIds properties changes (they're NOTIFYing
    // properties, see DockLayoutController.h) since areaIds() reads them
    // directly.
    readonly property var compactDockIds: dockLayout
        ? window.areaIds("left").concat(window.areaIds("top"),
                                         window.areaIds("bottom"),
                                         window.areaIds("right"))
        : []

    // Per-dock title/source, keyed by the same 8 ids DockLayoutController
    // (../DockLayoutController.h) uses for its xVisible/xFloating
    // properties and its leftDockIds/topDockIds/bottomDockIds/rightDockIds
    // area lists below. Feeds both the docked-area Repeaters (outerSplit,
    // below) and the FloatingDock instances further down, replacing what
    // used to be 8 separate hard-coded DockPanel{ title:...; source:...  }
    // blocks per fixed slot -- now a dock's *area* is data (dockLayout's
    // xArea, mutated via its "move to" menu -- see DockPanel.qml), so which
    // Repeater/container a panel's DockPanel instance lives under can
    // change at runtime instead of being fixed at compile time.
    readonly property var dockMeta: ({
        client: {title: qsTr("Client Panel"), source: "qrc:/qt/qml/MMapper/ClientPanel.qml"},
        group: {title: qsTr("Group Panel"), source: "qrc:/qt/qml/MMapper/GroupPanel.qml"},
        log: {title: qsTr("Log Panel"), source: "qrc:/qt/qml/MMapper/LogPanel.qml"},
        room: {title: qsTr("Room Panel"), source: "qrc:/qt/qml/MMapper/RoomPanel.qml"},
        adventure: {title: qsTr("Adventure Panel"),
                    source: "qrc:/qt/qml/MMapper/AdventurePanel.qml"},
        tasks: {title: qsTr("Tasks Panel"), source: "qrc:/qt/qml/MMapper/TasksPanel.qml"},
        description: {title: qsTr("Description Panel"),
                      source: "qrc:/qt/qml/MMapper/DescriptionPanel.qml"},
        timers: {title: qsTr("Timers Panel"), source: "qrc:/qt/qml/MMapper/TimerPanel.qml"}
    })

    // "dock" + capitalized id, matching each docked DockPanel's objectName
    // below (e.g. "dockLog") -- kept as one helper since both the
    // leftColumn/topRow/bottomRow/rightColumn Repeaters below and
    // TestQml.cpp's findChild<QQuickItem*>("dockLog"...) lookups need the
    // exact same mapping from id to objectName that the previous hard-coded
    // DockPanel blocks spelled out literally.
    function dockObjectName(id) {
        return "dock" + id.charAt(0).toUpperCase() + id.slice(1);
    }

    // TRANSPARENT-HOLE AUDIT (see MapCanvasUnderlayItem.h's file comment for
    // the underlay technique this exists for): MapView's canvas normally
    // draws the map into MainShell's ApplicationWindow.background --
    // QQC2.ApplicationWindow's default is an OPAQUE Rectangle filled from
    // the style palette, drawn as an ordinary scene-graph item BEHIND
    // contentItem but still AFTER (i.e. on top of, in the same render pass)
    // whatever MapCanvasUnderlayItem just drew directly into the window's
    // framebuffer during beforeRenderPassRecording() -- so left at its
    // default, that Rectangle would simply paint over the entire map every
    // frame. Replacing it with an empty (transparent) Item removes that
    // cover for the WHOLE window, not just the map's rect (a single flat
    // QML Rectangle can't be selectively "holed out" for one child item's
    // geometry) -- so every other visible piece of chrome must supply its
    // OWN opaque fill instead of relying on this one. Audited so far (each
    // gets its fill from the Fusion QQuickStyle set in main.cpp, or an
    // explicit Rectangle added alongside this change):
    //   - menuBar (QQC2.MenuBar below): Fusion style backgrounds it.
    //   - header's QQC2.ToolBar instances: Fusion style backgrounds them.
    //   - the 8 DockPanel side panels (../shell/DockPanel.qml): the header
    //     Rectangle{color: sysPalette.button} plus each panel's loaded
    //     content -- PanelFrame.qml (Rectangle{color: sysPalette.window})
    //     for 7 of them, ClientPanel.qml's own
    //     Rectangle{color: config.clientBgColor} for the 8th.
    //   - outerSplit's QQC2.SplitView handles (../shell/DockColumn.qml/
    //     DockRow.qml): Fusion style backgrounds them.
    //   - footer (below): was a bare Row with no fill of its own (relying
    //     entirely on this window's now-removed background) -- given an
    //     explicit Rectangle wrapper below.
    //   - ioProgressPopup (QQC2.Popup below): Fusion style backgrounds
    //     Popup's own background.
    //   - MapView.qml's own splash Image / scrollbars: deliberately left
    //     see-through where they don't paint -- that IS the map's rect, and
    //     is meant to show the underlay through (matching the previous
    //     MapCanvasItem/QQuickFramebufferObject behavior, where the splash
    //     simply sat on top of whatever the item's FBO contained).
    background: Item {}
    // windowTitle is a context property refreshed by
    // QmlShellWindow::updateWindowTitle() (see its doc comment for how it
    // mirrors MainWindow::setCurrentFile()); the literal fallback below only
    // matters before the very first setContextProperty() call in
    // QmlShellWindow's ctor, which happens before this file is loaded, so in
    // practice it's never seen.
    title: typeof windowTitle !== "undefined" ? windowTitle : qsTr("MMapper (QML shell preview)")

    // Mirrors MainWindow::closeEvent()'s maybeSave() gate: fires for both
    // the [X]/Alt+F4 titlebar close and file.exit's window->close() (see
    // QmlShellWindow.cpp's file.exit wiring, which routes through close()
    // instead of qApp->quit() specifically so this fires). shell.confirmClose()
    // (../../mainwindow/QmlShellWindow.h) runs the maybeSave() prompt (plus a
    // reduced "refuse to close while an IO task is running" guard -- see its
    // doc comment) and returns whether the close should proceed.
    onClosing: (close) => {
        if (typeof shell !== "undefined" && shell && !shell.confirmClose()) {
            close.accepted = false;
        }
    }

    // Forwards Escape to the canvas core, mirroring
    // MapWindow::keyPressEvent()'s Qt.Key_Escape handling in the widget
    // shell (see ../../display/mapwindow.cpp). MapCanvasCore::userPressedEscape()
    // ignores its bool argument (see MapCanvasCore.cpp), so a single
    // Shortcut firing on key-down (QML's Shortcut has no separate
    // key-release signal) is sufficient -- there is no press/release pair
    // to reproduce here, unlike the widget's keyPressEvent()/
    // keyReleaseEvent() pair.
    Shortcut {
        sequence: "Escape"
        onActivated: if (mapCore) {
            mapCore.userPressedEscape(true);
        }
    }

    component FileMenu: QQC2.Menu {
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

    component EditMenu: QQC2.Menu {
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

    component ViewMenu: QQC2.Menu {
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

    component ToolsMenu: QQC2.Menu {
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

    component HelpMenu: QQC2.Menu {
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

    menuBar: QQC2.MenuBar {
        // Mirrors MainWindow::slot_setShowMenuBar()'s menuBar()->show()/hide()
        // (mainwindow.cpp:1889-1919); the checked state is persisted to the
        // same Configuration::general.showMenuBar key (see
        // QmlShellWindow.cpp's view.show-menu-bar wiring). When the toggle is
        // off it can still be revealed by the top-edge peek (see
        // window.menuBarPeek and the menuPeekStrip below), which is this
        // shell's take on MainWindow's auto-reveal-on-hover.
        visible: (window.menuBarShown || window.menuBarPeek) && !window.compact

        // Keep the reveal up while the pointer is on the bar itself; hide
        // shortly after it leaves (unless it moved onto the peek strip).
        HoverHandler {
            id: menuBarHover
            onHoveredChanged: {
                if (hovered)
                    window.menuBarPeek = true;
                else
                    menuBarHideTimer.restart();
            }
        }

        FileMenu {}
        EditMenu {}
        ViewMenu {}
        ToolsMenu {}
        HelpMenu {}
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
                    implicitHeight: Theme.controlHeight
                    action: CommandAction { cmd: commands ? commands.command("file.new") : null }
                    QQC2.ToolTip.text: action ? action.toolTip : ""
                    QQC2.ToolTip.visible: hovered && action && action.toolTip.length > 0
                }
                QQC2.ToolButton {
                    implicitHeight: Theme.controlHeight
                    action: CommandAction { cmd: commands ? commands.command("file.open") : null }
                    QQC2.ToolTip.text: action ? action.toolTip : ""
                    QQC2.ToolTip.visible: hovered && action && action.toolTip.length > 0
                }
                QQC2.ToolButton {
                    implicitHeight: Theme.controlHeight
                    action: CommandAction { cmd: commands ? commands.command("file.save") : null }
                    QQC2.ToolTip.text: action ? action.toolTip : ""
                    QQC2.ToolTip.visible: hovered && action && action.toolTip.length > 0
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
                    implicitHeight: Theme.controlHeight
                    action: CommandAction { cmd: commands ? commands.command("mapper-mode.play") : null }
                    QQC2.ToolTip.text: action ? action.toolTip : ""
                    QQC2.ToolTip.visible: hovered && action && action.toolTip.length > 0
                }
                QQC2.ToolButton {
                    implicitHeight: Theme.controlHeight
                    action: CommandAction { cmd: commands ? commands.command("mapper-mode.map") : null }
                    QQC2.ToolTip.text: action ? action.toolTip : ""
                    QQC2.ToolTip.visible: hovered && action && action.toolTip.length > 0
                }
                QQC2.ToolButton {
                    implicitHeight: Theme.controlHeight
                    action: CommandAction {
                        cmd: commands ? commands.command("mapper-mode.offline") : null
                    }
                    QQC2.ToolTip.text: action ? action.toolTip : ""
                    QQC2.ToolTip.visible: hovered && action && action.toolTip.length > 0
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
                    implicitHeight: Theme.controlHeight
                    action: CommandAction { cmd: commands ? commands.command("mouse-mode.move") : null }
                    QQC2.ToolTip.text: action ? action.toolTip : ""
                    QQC2.ToolTip.visible: hovered && action && action.toolTip.length > 0
                }
                QQC2.ToolButton {
                    implicitHeight: Theme.controlHeight
                    action: CommandAction {
                        cmd: commands ? commands.command("mouse-mode.room-raypick") : null
                    }
                    QQC2.ToolTip.text: action ? action.toolTip : ""
                    QQC2.ToolTip.visible: hovered && action && action.toolTip.length > 0
                }
                QQC2.ToolButton {
                    implicitHeight: Theme.controlHeight
                    action: CommandAction {
                        cmd: commands ? commands.command("mouse-mode.room-select") : null
                    }
                    QQC2.ToolTip.text: action ? action.toolTip : ""
                    QQC2.ToolTip.visible: hovered && action && action.toolTip.length > 0
                }
                QQC2.ToolButton {
                    implicitHeight: Theme.controlHeight
                    action: CommandAction {
                        cmd: commands ? commands.command("mouse-mode.connection-select") : null
                    }
                    QQC2.ToolTip.text: action ? action.toolTip : ""
                    QQC2.ToolTip.visible: hovered && action && action.toolTip.length > 0
                }
                QQC2.ToolButton {
                    implicitHeight: Theme.controlHeight
                    action: CommandAction {
                        cmd: commands ? commands.command("mouse-mode.create-room") : null
                    }
                    QQC2.ToolTip.text: action ? action.toolTip : ""
                    QQC2.ToolTip.visible: hovered && action && action.toolTip.length > 0
                }
                QQC2.ToolButton {
                    implicitHeight: Theme.controlHeight
                    action: CommandAction {
                        cmd: commands ? commands.command("mouse-mode.create-connection") : null
                    }
                    QQC2.ToolTip.text: action ? action.toolTip : ""
                    QQC2.ToolTip.visible: hovered && action && action.toolTip.length > 0
                }
                QQC2.ToolButton {
                    implicitHeight: Theme.controlHeight
                    action: CommandAction {
                        cmd: commands
                              ? commands.command("mouse-mode.create-oneway-connection") : null
                    }
                    QQC2.ToolTip.text: action ? action.toolTip : ""
                    QQC2.ToolTip.visible: hovered && action && action.toolTip.length > 0
                }
                QQC2.ToolButton {
                    implicitHeight: Theme.controlHeight
                    action: CommandAction {
                        cmd: commands ? commands.command("mouse-mode.infomark-select") : null
                    }
                    QQC2.ToolTip.text: action ? action.toolTip : ""
                    QQC2.ToolTip.visible: hovered && action && action.toolTip.length > 0
                }
                QQC2.ToolButton {
                    implicitHeight: Theme.controlHeight
                    action: CommandAction {
                        cmd: commands ? commands.command("mouse-mode.create-infomark") : null
                    }
                    QQC2.ToolTip.text: action ? action.toolTip : ""
                    QQC2.ToolTip.visible: hovered && action && action.toolTip.length > 0
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
                    implicitHeight: Theme.controlHeight
                    action: CommandAction { cmd: commands ? commands.command("view.zoom-in") : null }
                    QQC2.ToolTip.text: action ? action.toolTip : ""
                    QQC2.ToolTip.visible: hovered && action && action.toolTip.length > 0
                }
                QQC2.ToolButton {
                    implicitHeight: Theme.controlHeight
                    action: CommandAction { cmd: commands ? commands.command("view.zoom-out") : null }
                    QQC2.ToolTip.text: action ? action.toolTip : ""
                    QQC2.ToolTip.visible: hovered && action && action.toolTip.length > 0
                }
                QQC2.ToolButton {
                    implicitHeight: Theme.controlHeight
                    action: CommandAction {
                        cmd: commands ? commands.command("view.zoom-reset") : null
                    }
                    QQC2.ToolTip.text: action ? action.toolTip : ""
                    QQC2.ToolTip.visible: hovered && action && action.toolTip.length > 0
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
                    implicitHeight: Theme.controlHeight
                    action: CommandAction { cmd: commands ? commands.command("layer.up") : null }
                    QQC2.ToolTip.text: action ? action.toolTip : ""
                    QQC2.ToolTip.visible: hovered && action && action.toolTip.length > 0
                }
                QQC2.ToolButton {
                    implicitHeight: Theme.controlHeight
                    action: CommandAction { cmd: commands ? commands.command("layer.down") : null }
                    QQC2.ToolTip.text: action ? action.toolTip : ""
                    QQC2.ToolTip.visible: hovered && action && action.toolTip.length > 0
                }
                QQC2.ToolButton {
                    implicitHeight: Theme.controlHeight
                    action: CommandAction { cmd: commands ? commands.command("layer.reset") : null }
                    QQC2.ToolTip.text: action ? action.toolTip : ""
                    QQC2.ToolTip.visible: hovered && action && action.toolTip.length > 0
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
                    implicitHeight: Theme.controlHeight
                    action: CommandAction {
                        cmd: commands ? commands.command("pathmachine.release-all-paths") : null
                    }
                    QQC2.ToolTip.text: action ? action.toolTip : ""
                    QQC2.ToolTip.visible: hovered && action && action.toolTip.length > 0
                }
                QQC2.ToolButton {
                    implicitHeight: Theme.controlHeight
                    action: CommandAction {
                        cmd: commands ? commands.command("room.goto-selected") : null
                    }
                    QQC2.ToolTip.text: action ? action.toolTip : ""
                    QQC2.ToolTip.visible: hovered && action && action.toolTip.length > 0
                }
                QQC2.ToolButton {
                    implicitHeight: Theme.controlHeight
                    action: CommandAction {
                        cmd: commands ? commands.command("room.force-update-selected") : null
                    }
                    QQC2.ToolTip.text: action ? action.toolTip : ""
                    QQC2.ToolTip.visible: hovered && action && action.toolTip.length > 0
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
                    implicitHeight: Theme.controlHeight
                    action: CommandAction { cmd: commands ? commands.command("room.find") : null }
                    QQC2.ToolTip.text: action ? action.toolTip : ""
                    QQC2.ToolTip.visible: hovered && action && action.toolTip.length > 0
                }
                QQC2.ToolButton {
                    implicitHeight: Theme.controlHeight
                    action: CommandAction { cmd: commands ? commands.command("room.edit-selected") : null }
                    QQC2.ToolTip.text: action ? action.toolTip : ""
                    QQC2.ToolTip.visible: hovered && action && action.toolTip.length > 0
                }
                QQC2.ToolButton {
                    implicitHeight: Theme.controlHeight
                    action: CommandAction {
                        cmd: commands ? commands.command("room.delete-selected") : null
                    }
                    QQC2.ToolTip.text: action ? action.toolTip : ""
                    QQC2.ToolTip.visible: hovered && action && action.toolTip.length > 0
                }
                QQC2.ToolButton {
                    implicitHeight: Theme.controlHeight
                    action: CommandAction {
                        cmd: commands ? commands.command("room.move-up-selected") : null
                    }
                    QQC2.ToolTip.text: action ? action.toolTip : ""
                    QQC2.ToolTip.visible: hovered && action && action.toolTip.length > 0
                }
                QQC2.ToolButton {
                    implicitHeight: Theme.controlHeight
                    action: CommandAction {
                        cmd: commands ? commands.command("room.move-down-selected") : null
                    }
                    QQC2.ToolTip.text: action ? action.toolTip : ""
                    QQC2.ToolTip.visible: hovered && action && action.toolTip.length > 0
                }
                QQC2.ToolButton {
                    implicitHeight: Theme.controlHeight
                    action: CommandAction {
                        cmd: commands ? commands.command("room.merge-up-selected") : null
                    }
                    QQC2.ToolTip.text: action ? action.toolTip : ""
                    QQC2.ToolTip.visible: hovered && action && action.toolTip.length > 0
                }
                QQC2.ToolButton {
                    implicitHeight: Theme.controlHeight
                    action: CommandAction {
                        cmd: commands ? commands.command("room.merge-down-selected") : null
                    }
                    QQC2.ToolTip.text: action ? action.toolTip : ""
                    QQC2.ToolTip.visible: hovered && action && action.toolTip.length > 0
                }
                QQC2.ToolButton {
                    implicitHeight: Theme.controlHeight
                    action: CommandAction {
                        cmd: commands ? commands.command("room.connect-to-neighbours") : null
                    }
                    QQC2.ToolTip.text: action ? action.toolTip : ""
                    QQC2.ToolTip.visible: hovered && action && action.toolTip.length > 0
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
                    implicitHeight: Theme.controlHeight
                    action: CommandAction {
                        cmd: commands ? commands.command("connection.delete-selected") : null
                    }
                    QQC2.ToolTip.text: action ? action.toolTip : ""
                    QQC2.ToolTip.visible: hovered && action && action.toolTip.length > 0
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
                    implicitHeight: Theme.controlHeight
                    action: CommandAction {
                        cmd: commands ? commands.command("edit.preferences") : null
                    }
                    QQC2.ToolTip.text: action ? action.toolTip : ""
                    QQC2.ToolTip.visible: hovered && action && action.toolTip.length > 0
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
    // Floating docks: a dock whose DockLayoutController xFloating property
    // is true (toggled via DockPanel.qml's float button, see
    // ../DockLayoutController.h) is pulled out of its fixed SplitView slot
    // above (the docked DockPanel instance below hides) and re-parented
    // into its own QtQuick.Window Loader instance instead (the FloatingDock
    // inline component below) -- mirrors QDockWidget::setFloating(true)'s
    // effect on the widget side, minus drag-to-float/drag-to-redock (the
    // float/re-dock button pair is this shell's equivalent gesture).
    //
    // Each fixed-slot container (leftColumn/rightColumn/topRow/bottomRow)
    // hides itself once none of its panels are docked-and-visible, so a
    // closed or floated-away sidebar gives its width/height back to the
    // center MapView -- SplitView skips invisible children entirely (their
    // preferredWidth/Height stops reserving space), but a container that
    // stays visible with only hidden/floating panels inside it does not,
    // which is why this can't be left to each DockPanel's own `visible`
    // alone. QQC2.SplitView.fillWidth on middleSplit (below) is what
    // actually reclaims the space once its siblings disappear.
    QQC2.SplitView {
        id: outerSplit
        objectName: "outerSplit"
        anchors.fill: parent
        orientation: Qt.Horizontal

        // The 4 area containers below are intentionally EMPTY in the markup:
        // their DockPanel children are placed imperatively by
        // reconcileDocks() (see it further down). Qt 6.4's QQC2.SplitView
        // does not adopt Repeater-generated delegates as split items (a
        // Repeater inside it creates nothing usable -- proven by TestQml), so
        // a declarative `Repeater { DockPanel {} }` per area simply renders no
        // docks. reconcileDocks() instead creates one DockPanel per
        // docked-and-shown dock directly in its area SplitView (via
        // insertItem()) and destroys it when the dock is hidden/floated/moved,
        // tracking DockLayoutController's leftDockIds/topDockIds/
        // bottomDockIds/rightDockIds -- create/destroy rather than reparenting
        // a persistent instance, see createDockPanel()'s comment for why.
        // Scope 13a: each area container is ALSO hidden whenever
        // window.compact is true, regardless of what it holds -- SplitView
        // skips invisible children entirely (see the comment above), so this
        // alone makes middleSplit/mapView fill the whole outerSplit in
        // compact mode; the hidden docks reappear in the compactDockDrawer
        // below instead. reconcileDocks() is untouched -- it still places
        // DockPanel instances into these containers even while they're
        // hidden, so nothing needs to be recreated when compact flips back
        // off.
        DockColumn {
            id: leftColumn
            objectName: "leftColumn"
            QQC2.SplitView.preferredWidth: 320
            visible: !window.compact && (dockLayout ? dockLayout.leftDockIds.length > 0 : false)
        }

        QQC2.SplitView {
            id: middleSplit
            orientation: Qt.Vertical
            QQC2.SplitView.fillWidth: true

            DockRow {
                id: topRow
                objectName: "topRow"
                QQC2.SplitView.preferredHeight: 220
                visible: !window.compact && (dockLayout ? dockLayout.topDockIds.length > 0 : false)
            }

            MapView {
                id: mapView
                QQC2.SplitView.fillHeight: true
                core: typeof mapCore !== "undefined" ? mapCore : null
            }

            DockRow {
                id: bottomRow
                objectName: "bottomRow"
                QQC2.SplitView.preferredHeight: 220
                visible: !window.compact && (dockLayout ? dockLayout.bottomDockIds.length > 0 : false)
            }
        }

        DockColumn {
            id: rightColumn
            objectName: "rightColumn"
            QQC2.SplitView.preferredWidth: 320
            visible: !window.compact && (dockLayout ? dockLayout.rightDockIds.length > 0 : false)
        }
    }

    // --- Docked-panel placement ------------------------------------------
    //
    // reconcileDocks() creates one DockPanel per docked-and-shown dock, in
    // its area SplitView (leftColumn/topRow/bottomRow/rightColumn), and
    // destroys it when the dock is hidden, floated, or moved elsewhere. See
    // the comment on the 4 empty containers above and createDockPanel().
    Component {
        id: dockPanelComponent
        DockPanel {}
    }

    // Transient creation parent: a DockPanel is created here (visible:false,
    // so it never flashes on screen) and then immediately insertItem()'d
    // into its area SplitView by createDockPanel(). It MUST be an explicit,
    // visible:false holder rather than creating the panel against the window
    // directly -- an Item created against the ApplicationWindow takes the
    // window's content item as its visual parent and would briefly render
    // stacked over the shell before being moved into its SplitView.
    Item {
        id: dockPoolHolder
        visible: false
    }

    // Edge hover zones + hide timers backing the menu-bar / status-bar peek
    // (see window.menuBarPeek/statusBarPeek and the menuBar/footer above). A
    // thin strip pinned to the window's top / bottom edge, z above outerSplit
    // so it wins the hover in that band; its HoverHandler is passive so map
    // clicks still fall through, and only enabled while the corresponding bar
    // is hidden. The bar's own HoverHandler keeps the reveal up once the
    // pointer moves onto it; the timer hides it a beat after the pointer has
    // left BOTH the strip and the bar (so sliding from strip to bar doesn't
    // flicker it away).
    Item {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 4
        z: 10000
        HoverHandler {
            id: menuPeekStrip
            enabled: !window.menuBarShown
            onHoveredChanged: {
                if (hovered)
                    window.menuBarPeek = true;
                else
                    menuBarHideTimer.restart();
            }
        }
    }
    Item {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 4
        z: 10000
        HoverHandler {
            id: statusPeekStrip
            enabled: !window.statusBarShown
            onHoveredChanged: {
                if (hovered)
                    window.statusBarPeek = true;
                else
                    statusBarHideTimer.restart();
            }
        }
    }
    Timer {
        id: menuBarHideTimer
        interval: 400
        onTriggered: {
            if (!menuBarHover.hovered && !menuPeekStrip.hovered)
                window.menuBarPeek = false;
        }
    }
    Timer {
        id: statusBarHideTimer
        interval: 400
        onTriggered: {
            if (!statusBarHover.hovered && !statusPeekStrip.hovered)
                window.statusBarPeek = false;
        }
    }

    // dockId -> the DockPanel instance CURRENTLY placed in an area SplitView
    // (only docked-and-shown panels have an entry; hidden/floated ones have
    // none). reconcileDocks() creates and destroys these to match the
    // controller's area lists -- see its comment for why placement is
    // create/destroy rather than reparenting.
    property var dockInstances: ({})

    function areaSplitView(area) {
        if (area === "left")
            return leftColumn;
        if (area === "top")
            return topRow;
        if (area === "bottom")
            return bottomRow;
        if (area === "right")
            return rightColumn;
        return null;
    }

    function areaIds(area) {
        if (!dockLayout)
            return [];
        if (area === "left")
            return dockLayout.leftDockIds;
        if (area === "top")
            return dockLayout.topDockIds;
        if (area === "bottom")
            return dockLayout.bottomDockIds;
        if (area === "right")
            return dockLayout.rightDockIds;
        return [];
    }

    function createDockPanel(id, area, sv, index) {
        const meta = window.dockMeta[id];
        // Create parented to the invisible holder first, then insertItem()
        // moves it into the area SplitView -- this is exactly the path that
        // renders correctly for the panels shown at startup. We deliberately
        // do NOT keep one long-lived instance and reparent it between
        // SplitViews on a move: reparenting a live QQuickItem across
        // SplitViews leaves a stale scene-graph node on the OpenGL backend,
        // so the moved panel reserves layout space but never actually draws
        // (it looks correct in the software-rendered headless tests, which is
        // why this only showed up on the real GL shell). Creating a fresh
        // instance in the target and destroying the old one sidesteps that
        // entirely. The cost is that a panel's transient view state (scroll
        // position, unapplied filter text) resets on a move or a hide/show --
        // the same tradeoff a float/re-dock already makes; the panel's real
        // data lives in its C++ model/controller, not the delegate.
        const inst = dockPanelComponent.createObject(dockPoolHolder, {
            objectName: window.dockObjectName(id),
            dockId: id,
            title: meta ? meta.title : "",
            source: meta ? meta.source : "",
            currentArea: area
        });
        inst.closeRequested.connect(function () {
            if (dockLayout)
                dockLayout[id + "Visible"] = false;
        });
        inst.floatRequested.connect(function () {
            if (dockLayout)
                dockLayout[id + "Floating"] = true;
        });
        inst.moveToAreaRequested.connect(function (targetArea) {
            if (dockLayout)
                dockLayout.setDockArea(id, targetArea);
        });
        sv.insertItem(index, inst);
        return inst;
    }

    function reconcileDocks() {
        if (!dockLayout)
            return;
        const areas = ["left", "top", "bottom", "right"];
        var a, i;

        // Which area (if any) each id should be docked-and-shown in now.
        var wantedArea = ({});
        for (a = 0; a < areas.length; ++a) {
            const ids = areaIds(areas[a]);
            for (i = 0; i < ids.length; ++i)
                wantedArea[ids[i]] = areas[a];
        }

        // Destroy any placed instance that is no longer wanted, or is wanted
        // in a DIFFERENT area (the move case -- destroy here, recreate fresh
        // in the target below).
        for (var id in window.dockInstances) {
            const inst = window.dockInstances[id];
            if (!inst) {
                delete window.dockInstances[id];
                continue;
            }
            if (wantedArea[id] !== inst.currentArea) {
                const sv = areaSplitView(inst.currentArea);
                if (sv)
                    sv.removeItem(inst);
                inst.destroy();
                delete window.dockInstances[id];
            }
        }

        // Ensure each area's SplitView holds exactly its ids, in canonical
        // order. A missing id is created fresh at its slot; an id already
        // present but at the wrong index only happens when a sibling was
        // added/removed, and removeItem()/insertItem() auto-shift the rest,
        // so by creating in index order the survivors already sit correctly.
        for (a = 0; a < areas.length; ++a) {
            const sv2 = areaSplitView(areas[a]);
            const want = areaIds(areas[a]);
            for (i = 0; i < want.length; ++i) {
                const wantId = want[i];
                const existing = window.dockInstances[wantId];
                if (existing && i < sv2.count && sv2.contentChildren[i] === existing)
                    continue;
                if (!existing)
                    window.dockInstances[wantId] = createDockPanel(wantId, areas[a], sv2, i);
            }
        }
    }

    Connections {
        target: dockLayout
        ignoreUnknownSignals: true
        function onLeftDockIdsChanged() {
            window.reconcileDocks();
        }
        function onTopDockIdsChanged() {
            window.reconcileDocks();
        }
        function onBottomDockIdsChanged() {
            window.reconcileDocks();
        }
        function onRightDockIdsChanged() {
            window.reconcileDocks();
        }
    }

    Component.onCompleted: {
        window.reconcileDocks();
    }

    // Inline component backing every floating dock's top-level window (Qt
    // 6.4+ `component ... : Base { ... }` syntax) -- one Loader per dock,
    // instantiated below, rather than 8 near-identical Window blocks
    // spelled out longhand. `active` only becomes true once a dock is both
    // visible and floating, so a hidden-and-floating dock (closed while
    // floating) doesn't leave a stray empty Window around, and re-docking
    // (floating -> false) tears the Window down rather than merely hiding
    // it -- the content Loader/DockPanel re-instantiates on every dock<->
    // float transition either way, so any panel-local UI state (scroll
    // position, filter text, ...) is lost across a float/re-dock just like
    // it would be across a close/reopen; this is a deliberate simplicity
    // tradeoff (see the task report), not a regression from the SplitView-
    // only layout this replaces.
    component FloatingDock: Loader {
        id: floatLoader

        // Property-name prefix into DockLayoutController's per-dock
        // xVisible/xFloating/xFloatGeometry properties (e.g. "log" ->
        // logVisible/logFloating/logFloatGeometry) -- looked up with JS
        // bracket notation below so this one component can back all 8
        // docks instead of one bespoke Window block per dock.
        required property string dockId
        required property string dockTitle
        required property url dockSource

        active: dockLayout
                ? (dockLayout[dockId + "Visible"] === true
                   && dockLayout[dockId + "Floating"] === true)
                : false

        // sourceComponent needs an explicit `Component { Window { ... } }`
        // wrapper here -- the usual QML shorthand of assigning an object
        // literal straight to a QQmlComponent-typed property (letting the
        // engine implicitly wrap it in a Component) does not apply inside
        // an inline `component ... : Base { ... }` definition (Qt 6.4;
        // reproduced as a minimal standalone case while building this),
        // even though it works fine for a Loader declared directly in a
        // regular (non-inline) object tree -- hence the explicit wrapper
        // below, unlike a typical top-level Loader.sourceComponent.
        sourceComponent: Component {
            Window {
                id: floatWindow
                flags: Qt.Tool
                title: floatLoader.dockTitle
                width: 360
                height: 480
                x: 100
                y: 100
                visible: true

                // Applies the last-saved geometry (if any) once, instead of
                // a live `width:`/`x:`-style binding -- a binding would
                // fight with the OS/user resizing or moving this Window
                // afterwards (each drag imperatively writes x/y/width/
                // height, which is exactly what the onXChanged/... handlers
                // below persist, but a live binding back to the same
                // property the user's drag just wrote would either no-op
                // or reintroduce a feedback loop) -- applying once on
                // completion and then persisting imperative changes is the
                // simpler, reliable option the task asked for.
                Component.onCompleted: {
                    if (!dockLayout) {
                        return;
                    }
                    const g = dockLayout[floatLoader.dockId + "FloatGeometry"];
                    if (g && g.width > 0 && g.height > 0) {
                        x = g.x;
                        y = g.y;
                        width = g.width;
                        height = g.height;
                    }
                }

                function saveGeometry() {
                    if (dockLayout) {
                        dockLayout[floatLoader.dockId + "FloatGeometry"] = Qt.rect(x, y, width,
                                                                                    height);
                    }
                }
                onXChanged: saveGeometry()
                onYChanged: saveGeometry()
                onWidthChanged: saveGeometry()
                onHeightChanged: saveGeometry()

                // Closing the floating window (native close button, Alt+F4,
                // ...) is equivalent to clicking DockPanel's close button --
                // hides the dock entirely rather than silently re-docking
                // it.
                onClosing: if (dockLayout) dockLayout[floatLoader.dockId + "Visible"] = false

                DockPanel {
                    anchors.fill: parent
                    dockId: floatLoader.dockId
                    title: floatLoader.dockTitle
                    source: floatLoader.dockSource
                    floating: true
                    // A floating panel has no single "current area" to grey
                    // out (it isn't docked anywhere), so leave currentArea
                    // at "" -- every entry stays enabled -- and treat
                    // picking one as "re-dock me into that area": set the
                    // area and clear floating so the docked-area Repeater
                    // above picks the panel back up.
                    currentArea: ""
                    onMoveToAreaRequested: (area) => {
                        if (dockLayout) {
                            dockLayout.setDockArea(floatLoader.dockId, area);
                            dockLayout[floatLoader.dockId + "Floating"] = false;
                        }
                    }
                    onFloatRequested: if (dockLayout)
                                           dockLayout[floatLoader.dockId + "Floating"] = false
                    onCloseRequested: if (dockLayout)
                                           dockLayout[floatLoader.dockId + "Visible"] = false
                }
            }
        }
    }

    FloatingDock {
        objectName: "floatLog"
        dockId: "log"
        dockTitle: qsTr("Log Panel")
        dockSource: "qrc:/qt/qml/MMapper/LogPanel.qml"
    }
    FloatingDock {
        objectName: "floatGroup"
        dockId: "group"
        dockTitle: qsTr("Group Panel")
        dockSource: "qrc:/qt/qml/MMapper/GroupPanel.qml"
    }
    FloatingDock {
        objectName: "floatRoom"
        dockId: "room"
        dockTitle: qsTr("Room Panel")
        dockSource: "qrc:/qt/qml/MMapper/RoomPanel.qml"
    }
    FloatingDock {
        objectName: "floatAdventure"
        dockId: "adventure"
        dockTitle: qsTr("Adventure Panel")
        dockSource: "qrc:/qt/qml/MMapper/AdventurePanel.qml"
    }
    FloatingDock {
        objectName: "floatDescription"
        dockId: "description"
        dockTitle: qsTr("Description Panel")
        dockSource: "qrc:/qt/qml/MMapper/DescriptionPanel.qml"
    }
    FloatingDock {
        objectName: "floatTimers"
        dockId: "timers"
        dockTitle: qsTr("Timers Panel")
        dockSource: "qrc:/qt/qml/MMapper/TimerPanel.qml"
    }
    FloatingDock {
        objectName: "floatTasks"
        dockId: "tasks"
        dockTitle: qsTr("Tasks Panel")
        dockSource: "qrc:/qt/qml/MMapper/TasksPanel.qml"
    }
    FloatingDock {
        objectName: "floatClient"
        dockId: "client"
        dockTitle: qsTr("Client Panel")
        dockSource: "qrc:/qt/qml/MMapper/ClientPanel.qml"
    }

    // Footer status bar row, mirroring MainWindow::setupStatusBar():
    // ClockStrip.qml/XpStatusItem.qml permanent widgets (inserted at index
    // 0, i.e. leftmost of the permanent widgets, which visually places them
    // at the status bar's right edge -- see QStatusBar::insertPermanentWidget()'s
    // docs), a path-machine QLabel placeholder (no PathMachine wired into
    // this offline shell yet -- see the TODO below), and the transient
    // message label filling the remaining space on the left, exactly like
    // QStatusBar::showMessage()'s text does relative to permanent widgets.
    footer: Rectangle {
        id: footerBar
        // Explicit opaque fill: see the ApplicationWindow.background
        // "TRANSPARENT-HOLE AUDIT" comment above -- this row previously
        // relied entirely on that now-removed background Rectangle to look
        // opaque.
        // Mirrors MainWindow's statusBar()->setVisible(showStatusBar): hidden
        // when the "Always Show Status Bar" toggle (view.show-status-bar) is
        // unchecked -- but still revealable by the bottom-edge peek (see
        // window.statusBarPeek and statusPeekStrip below). ApplicationWindow
        // gives a hidden footer no height, so this reclaims the space too.
        visible: window.statusBarShown || window.statusBarPeek
        implicitHeight: footerRow.implicitHeight
        color: footerPalette.window

        HoverHandler {
            id: statusBarHover
            onHoveredChanged: {
                if (hovered)
                    window.statusBarPeek = true;
                else
                    statusBarHideTimer.restart();
            }
        }

        SystemPalette {
            id: footerPalette
            colorGroup: SystemPalette.Active
        }

        Row {
            id: footerRow
            anchors.fill: parent
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
                // Fed by Mmapper2PathMachine::sig_state via QmlShellWindow.cpp's
                // wirePathMachine(), which keeps the "pathMachineStatus" context
                // property in sync (mirrors MainWindow::setupStatusBar()'s
                // `connect(m_pathMachine, &Mmapper2PathMachine::sig_state,
                // pathmachineStatus, &QLabel::setText)`).
                text: typeof pathMachineStatus !== "undefined" ? pathMachineStatus : ""
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

    // Async IO progress popup: file.open/new/merge/reload/save/export all
    // run on a background thread via async_tasks (see QmlShellWindow.cpp's
    // loadFile()/mergeFile()/saveMapFile()), which previously showed no
    // feedback beyond the Tasks panel -- hidden by default (see
    // DockLayoutController.h's tasksVisible), so loads/saves appeared to do
    // nothing. This modal popup is the visible equivalent of
    // MainWindow::AsyncIO's QProgressDialog (mainwindow-async.cpp's
    // createNewProgressDialog()), driven by IoTaskController
    // (../../mainwindow/IoTaskController.h) via the "ioTask" context
    // property: visible for as long as ioTask.active is true, with a Cancel
    // button enabled only for cancelable tasks (loads/merges; saves forbid
    // cancel, mirroring createNewProgressDialog()'s disabled Cancel button
    // for AsyncIOTypeEnum::Save).
    QQC2.Popup {
        id: ioProgressPopup
        objectName: "ioProgressPopup"
        parent: window.contentItem
        anchors.centerIn: parent
        modal: true
        // No click-outside/Escape dismissal, matching QProgressDialog's own
        // modal behavior -- the only way out is the task finishing or
        // (when cancelable) the Cancel button below.
        closePolicy: QQC2.Popup.NoAutoClose
        visible: typeof ioTask !== "undefined" && ioTask ? ioTask.active : false
        width: 360

        contentItem: Column {
            spacing: 8
            width: 340

            QQC2.Label {
                id: ioProgressLabel
                objectName: "ioProgressLabel"
                width: parent.width
                wrapMode: Text.WordWrap
                text: typeof ioTask !== "undefined" && ioTask ? ioTask.label : ""
            }
            QQC2.ProgressBar {
                id: ioProgressBar
                objectName: "ioProgressBar"
                width: parent.width
                from: 0
                to: 100
                value: typeof ioTask !== "undefined" && ioTask ? ioTask.percent : 0
            }
            QQC2.Button {
                id: ioProgressCancelButton
                objectName: "ioProgressCancelButton"
                text: qsTr("Cancel")
                enabled: typeof ioTask !== "undefined" && ioTask ? ioTask.cancelable : false
                onClicked: if (ioTask) ioTask.cancel()
            }
        }
    }

    // --- Scope 13a: compact/mobile dock drawer -----------------------------
    //
    // In compact mode the 4 fixed-slot dock areas are hidden (see their
    // visible: bindings above), so the docks they would have held (Group,
    // Log, Room, etc.) have nowhere to be seen -- this Drawer is that
    // somewhere. It's a sibling of outerSplit (a direct child of window, NOT
    // inside outerSplit), so it slides in over the full-screen map rather
    // than fighting it for SplitView space. compactDockButton below is its
    // only opener; it never auto-opens.
    QQC2.Drawer {
        id: compactDockDrawer
        objectName: "compactDockDrawer"
        edge: Qt.BottomEdge
        width: window.width
        height: window.height * 0.6
        // Swiping it open from a hidden edge only makes sense in compact
        // mode -- on desktop this stays uninteractive so it can never be
        // dragged into view by accident.
        interactive: window.compact

        // A drawer left open across a resize/rotate back to desktop width
        // must not stay open floating over the now-restored dock columns --
        // force it closed the instant compact goes false. (compact lives on
        // window, not on this Drawer, so this has to be a Connections block
        // rather than an onCompactChanged handler here.)
        Connections {
            target: window
            function onCompactChanged() {
                if (!window.compact)
                    compactDockDrawer.close();
            }
        }

        // Index into window.compactDockIds selected by the tab row below;
        // clamped whenever the list changes (a dock can close, move, or
        // float away while the drawer is open) so it never points past the
        // end -- and never goes negative when the list becomes empty.
        property int selectedIndex: 0
        function clampSelectedIndex() {
            const n = window.compactDockIds.length;
            if (n === 0) {
                compactDockDrawer.selectedIndex = 0;
            } else if (compactDockDrawer.selectedIndex >= n) {
                compactDockDrawer.selectedIndex = n - 1;
            } else if (compactDockDrawer.selectedIndex < 0) {
                compactDockDrawer.selectedIndex = 0;
            }
        }
        Connections {
            target: window
            function onCompactDockIdsChanged() { compactDockDrawer.clampSelectedIndex(); }
        }

        SystemPalette {
            id: drawerPalette
            colorGroup: SystemPalette.Active
        }

        // Explicit opaque fill -- see the ApplicationWindow.background
        // "TRANSPARENT-HOLE AUDIT" comment above; Drawer's own background is
        // themed by the Fusion style, but the Column below is content we add
        // on top of it, not styled chrome, so it gets its own Rectangle too.
        Rectangle {
            anchors.fill: parent
            color: drawerPalette.window

            Column {
                anchors.fill: parent

                QQC2.TabBar {
                    id: compactDockTabBar
                    objectName: "compactDockTabBar"
                    width: parent.width
                    visible: window.compactDockIds.length > 0
                    currentIndex: compactDockDrawer.selectedIndex
                    onCurrentIndexChanged: compactDockDrawer.selectedIndex = currentIndex

                    Repeater {
                        model: window.compactDockIds
                        delegate: QQC2.TabButton {
                            required property string modelData
                            text: window.dockMeta[modelData]
                                  ? window.dockMeta[modelData].title : modelData
                        }
                    }
                }

                QQC2.Label {
                    objectName: "compactDockEmptyLabel"
                    width: parent.width
                    height: window.compactDockIds.length === 0 ? parent.height : 0
                    visible: window.compactDockIds.length === 0
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    text: qsTr("No panels open")
                }

                QQC2.Frame {
                    id: compactDockFrame
                    width: parent.width
                    height: parent.height - compactDockTabBar.height
                    visible: window.compactDockIds.length > 0

                    Loader {
                        id: compactDockLoader
                        objectName: "compactDockLoader"
                        anchors.fill: parent
                        source: {
                            const ids = window.compactDockIds;
                            const idx = compactDockDrawer.selectedIndex;
                            if (idx < 0 || idx >= ids.length)
                                return "";
                            const meta = window.dockMeta[ids[idx]];
                            return meta ? meta.source : "";
                        }
                    }
                }
            }
        }
    }

    // Compact-only floating affordance that opens compactDockDrawer -- the
    // drawer has no other opener (its edge-swipe gesture is easy to miss on
    // a full-screen map that also wants drag gestures for panning). Sits
    // above outerSplit (z: 9000) but below the menu/status-bar peek strips
    // (z: 10000) so it never steals those edge hovers.
    QQC2.RoundButton {
        id: compactDockButton
        objectName: "compactDockButton"
        visible: window.compact
        z: 9000
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 16
        implicitWidth: Theme.controlHeight
        implicitHeight: Theme.controlHeight
        text: "☰"
        onClicked: compactDockDrawer.open()
    }

    // Compact-mode replacement for the (now-hidden, see menuBar's
    // `visible:` binding above) horizontal QQC2.MenuBar: the same 6
    // top-level menus, reused verbatim via the FileMenu/EditMenu/ViewMenu/
    // ToolsMenu/HelpMenu inline components above rather than duplicating
    // their ~600 lines of CommandAction-wired items, just nested as
    // submenus of one popup Menu instead of laid out side-by-side in a
    // MenuBar. QQC2.Menu accepts Menu children as cascading submenus in Qt
    // 6.4 (tap/click opens them), which is an acceptable touch affordance
    // here even though it is one extra tap deep versus a MenuBar.
    QQC2.Menu {
        id: appMenu
        objectName: "appMenu"

        FileMenu {}
        EditMenu {}
        ViewMenu {}
        ToolsMenu {}
        HelpMenu {}
    }

    // Compact-only opener for appMenu above, replacing the hidden MenuBar.
    // Top-left placement (mirroring where a hamburger conventionally sits)
    // keeps it clear of compactDockButton's bottom-right corner. z matches
    // compactDockButton's -- both float above outerSplit but below the
    // menu/status-bar peek strips (z: 10000).
    QQC2.ToolButton {
        id: hamburgerButton
        objectName: "hamburgerButton"
        visible: window.compact
        z: 9000
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.margins: 16
        implicitWidth: Theme.controlHeight
        implicitHeight: Theme.controlHeight
        text: "☰"
        onClicked: appMenu.popup()
    }
}
