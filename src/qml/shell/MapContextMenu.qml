// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

import QtQuick
import QtQuick.Controls as QQC2
import MMapper

// The map canvas's right-click context menu, mirroring
// MainWindow::slot_showContextMenu() (../../mainwindow/mainwindow.cpp):
// which section is shown depends on the current selection kind (connection
// selected / rooms empty vs non-empty / infomarks), plus a nested "Mouse
// Mode" submenu (same 9 commands as the menu bar's individual mouse-mode
// items, gathered together here the way slot_showContextMenu() gathers
// them). Instantiated once by MapView.qml, positioned at the canvas-local
// point MapCanvasCore::sig_customContextMenuRequested carries (MapView's
// MapCanvasItem fills its parent with no offset -- see MapView.qml -- so
// that point is already in this item's own parent's coordinate space, hence
// `parent: core`-less popup(x, y) below).
//
// Selection-state bindings come from the "shell" context property
// (QmlShellWindow's hasRoomSelection/roomSelectionEmpty/
// hasConnectionSelection/hasInfomarkSelection/infomarkSelectionEmpty
// Q_PROPERTYs -- see ../../mainwindow/QmlShellWindow.h) rather than from
// `core` directly: MapCanvasCore only emits transient sig_newRoomSelection/
// sig_newConnectionSelection/sig_newInfomarkSelection signals, it doesn't
// hold the selection state itself (QmlShellWindow does, exactly like
// MainWindow's m_roomSelection/m_connectionSelection/m_infoMarkSelection --
// see QmlShellWindow.h's doc comment on those members).
QQC2.Menu {
    id: root

    // MapCanvasCore*, bound by MapView.qml; used only to receive
    // sig_customContextMenuRequested/sig_dismissContextMenu (see the
    // Connections block below) -- selection state comes from `shell`
    // instead (see the file comment above).
    property var core: null

    readonly property bool hasConnectionSelection:
        typeof shell !== "undefined" && shell ? shell.hasConnectionSelection : false
    readonly property bool hasRoomSelection:
        typeof shell !== "undefined" && shell ? shell.hasRoomSelection : false
    readonly property bool roomSelectionEmpty:
        typeof shell !== "undefined" && shell ? shell.roomSelectionEmpty : true
    readonly property bool hasInfomarkSelection:
        typeof shell !== "undefined" && shell ? shell.hasInfomarkSelection : false
    readonly property bool infomarkSelectionEmpty:
        typeof shell !== "undefined" && shell ? shell.infomarkSelectionEmpty : true

    // Mirrors slot_showContextMenu()'s `if (m_connectionSelection != nullptr)
    // ... else { if (m_roomSelection != nullptr) ... }`: a connection
    // selection forecloses the room/infomark sections entirely.
    readonly property bool showCreateRoom:
        !hasConnectionSelection && hasRoomSelection && roomSelectionEmpty
    readonly property bool showRoomOps:
        !hasConnectionSelection && hasRoomSelection && !roomSelectionEmpty
    readonly property bool showInfomarkOps:
        !hasConnectionSelection && hasInfomarkSelection && !infomarkSelectionEmpty

    Connections {
        target: root.core
        function onSig_customContextMenuRequested(pos) {
            root.popup(pos.x, pos.y)
        }
        function onSig_dismissContextMenu() {
            root.close()
        }
    }

    // --- connection selected ---
    QQC2.MenuItem {
        objectName: "ctxDeleteConnection"
        visible: root.hasConnectionSelection
        height: visible ? implicitHeight : 0
        action: CommandAction {
            cmd: commands ? commands.command("connection.delete-selected") : null
        }
    }

    // --- rooms: empty selection -> create ---
    QQC2.MenuItem {
        objectName: "ctxCreateRoom"
        visible: root.showCreateRoom
        height: visible ? implicitHeight : 0
        action: CommandAction { cmd: commands ? commands.command("room.create") : null }
    }

    // --- rooms: non-empty selection -> the full room operation set ---
    QQC2.MenuItem {
        objectName: "ctxEditRoom"
        visible: root.showRoomOps
        height: visible ? implicitHeight : 0
        action: CommandAction { cmd: commands ? commands.command("room.edit-selected") : null }
    }
    QQC2.MenuItem {
        objectName: "ctxMoveUpRoom"
        visible: root.showRoomOps
        height: visible ? implicitHeight : 0
        action: CommandAction {
            cmd: commands ? commands.command("room.move-up-selected") : null
        }
    }
    QQC2.MenuItem {
        objectName: "ctxMoveDownRoom"
        visible: root.showRoomOps
        height: visible ? implicitHeight : 0
        action: CommandAction {
            cmd: commands ? commands.command("room.move-down-selected") : null
        }
    }
    QQC2.MenuItem {
        objectName: "ctxMergeUpRoom"
        visible: root.showRoomOps
        height: visible ? implicitHeight : 0
        action: CommandAction {
            cmd: commands ? commands.command("room.merge-up-selected") : null
        }
    }
    QQC2.MenuItem {
        objectName: "ctxMergeDownRoom"
        visible: root.showRoomOps
        height: visible ? implicitHeight : 0
        action: CommandAction {
            cmd: commands ? commands.command("room.merge-down-selected") : null
        }
    }
    QQC2.MenuItem {
        objectName: "ctxDeleteRoom"
        visible: root.showRoomOps
        height: visible ? implicitHeight : 0
        action: CommandAction {
            cmd: commands ? commands.command("room.delete-selected") : null
        }
    }
    QQC2.MenuItem {
        objectName: "ctxConnectToNeighbours"
        visible: root.showRoomOps
        height: visible ? implicitHeight : 0
        action: CommandAction {
            cmd: commands ? commands.command("room.connect-to-neighbours") : null
        }
    }
    QQC2.MenuSeparator {
        objectName: "ctxRoomOpsSeparator"
        visible: root.showRoomOps
        height: visible ? implicitHeight : 0
    }
    QQC2.MenuItem {
        objectName: "ctxGotoRoom"
        visible: root.showRoomOps
        height: visible ? implicitHeight : 0
        action: CommandAction { cmd: commands ? commands.command("room.goto-selected") : null }
    }
    QQC2.MenuItem {
        objectName: "ctxForceUpdateRoom"
        visible: root.showRoomOps
        height: visible ? implicitHeight : 0
        action: CommandAction {
            cmd: commands ? commands.command("room.force-update-selected") : null
        }
    }

    // --- infomarks (can be shown alongside rooms) ---
    QQC2.MenuSeparator {
        objectName: "ctxInfomarkSeparator"
        visible: root.showInfomarkOps && root.hasRoomSelection
        height: visible ? implicitHeight : 0
    }
    QQC2.MenuItem {
        objectName: "ctxEditInfomark"
        visible: root.showInfomarkOps
        height: visible ? implicitHeight : 0
        action: CommandAction { cmd: commands ? commands.command("infomark.edit-selected") : null }
    }
    QQC2.MenuItem {
        objectName: "ctxDeleteInfomark"
        visible: root.showInfomarkOps
        height: visible ? implicitHeight : 0
        action: CommandAction {
            cmd: commands ? commands.command("infomark.delete-selected") : null
        }
    }

    QQC2.MenuSeparator {}

    // --- Mouse Mode submenu -- mirrors slot_showContextMenu()'s nested
    // "Mouse Mode" QMenu, same 9 commands the menu bar's individual Markers/
    // Rooms/Connections/View menus expose one at a time (see MainShell.qml).
    QQC2.Menu {
        objectName: "ctxMouseModeMenu"
        title: qsTr("Mouse Mode")

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
                cmd: commands ? commands.command("mouse-mode.infomark-select") : null
            }
        }
        QQC2.MenuItem {
            action: CommandAction {
                cmd: commands ? commands.command("mouse-mode.connection-select") : null
            }
        }
        QQC2.MenuItem {
            action: CommandAction {
                cmd: commands ? commands.command("mouse-mode.create-infomark") : null
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
    }
}
