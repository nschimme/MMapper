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

    MapView {
        id: mapView
        anchors.fill: parent
        core: typeof mapCore !== "undefined" ? mapCore : null
    }

    footer: QQC2.Label {
        objectName: "statusLabel"
        text: typeof statusText !== "undefined" ? statusText : ""
        padding: 4
    }
}
