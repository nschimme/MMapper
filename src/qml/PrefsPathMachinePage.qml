// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

import QtQuick
import QtQuick.Controls

import MMapper

// Path Machine preferences page, driven by
// preferencesController.pathMachine (a PathMachinePageAdapter, see
// ../preferences/PathMachinePageAdapter.h). Mirrors pathmachinepage.ui's
// field set without a .ui file.
//
// QtQuick.Controls' SpinBox (Qt 6.4, packaged module) is integer-only: it
// has no "decimals"/double value support like QDoubleSpinBox. Rather than
// the scaled-integer recipe from Qt's own SpinBox docs (storing value*10^n
// as an int, which risks off-by-one rounding bugs in textFromValue/
// valueFromText), each double field here is a plain TextField constrained
// by a DoubleValidator(0, 100, 4) (matching acceptBestRelativeDoubleSpinBox
// et al.'s 0..100 range / 4 decimals in pathmachinepage.ui) that commits to
// the adapter on editingFinished. This is the pattern used for every
// double-valued field across the five ported pages.
Flickable {
    id: root
    clip: true
    contentWidth: width
    contentHeight: column.implicitHeight

    readonly property var pathMachine: preferencesController.pathMachine

    Column {
        id: column
        width: root.width
        spacing: 8

        Label { text: qsTr("WARNING: These settings are for advanced users only"); font.bold: true }

        Row {
            spacing: 8
            Label { text: qsTr("Max Paths:"); width: 180 }
            SpinBox {
                id: maxPathsBox
                from: 0
                to: 100000
                value: root.pathMachine.maxPaths
                onValueModified: root.pathMachine.maxPaths = value
            }
        }

        Row {
            spacing: 8
            Label { text: qsTr("Matching Tolerance:"); width: 180 }
            SpinBox {
                id: matchingToleranceBox
                from: 0
                to: 100
                value: root.pathMachine.matchingTolerance
                onValueModified: root.pathMachine.matchingTolerance = value
            }
        }

        Row {
            spacing: 8
            Label { text: qsTr("Accept Best Relative:"); width: 180 }
            TextField {
                id: acceptBestRelativeField
                width: 100
                text: root.pathMachine.acceptBestRelative.toFixed(4)
                validator: DoubleValidator { bottom: 0; top: 100; decimals: 4 }
                onEditingFinished: root.pathMachine.acceptBestRelative = parseFloat(text)
            }
        }

        Row {
            spacing: 8
            Label { text: qsTr("Accept Best Absolute:"); width: 180 }
            TextField {
                id: acceptBestAbsoluteField
                width: 100
                text: root.pathMachine.acceptBestAbsolute.toFixed(4)
                validator: DoubleValidator { bottom: 0; top: 100; decimals: 4 }
                onEditingFinished: root.pathMachine.acceptBestAbsolute = parseFloat(text)
            }
        }

        Row {
            spacing: 8
            Label { text: qsTr("New Room Penalty:"); width: 180 }
            TextField {
                id: newRoomPenaltyField
                width: 100
                text: root.pathMachine.newRoomPenalty.toFixed(4)
                validator: DoubleValidator { bottom: 0; top: 100; decimals: 4 }
                onEditingFinished: root.pathMachine.newRoomPenalty = parseFloat(text)
            }
        }

        Row {
            spacing: 8
            Label { text: qsTr("Correct Position bonus:"); width: 180 }
            TextField {
                id: correctPositionBonusField
                width: 100
                text: root.pathMachine.correctPositionBonus.toFixed(4)
                validator: DoubleValidator { bottom: 0; top: 100; decimals: 4 }
                onEditingFinished: root.pathMachine.correctPositionBonus = parseFloat(text)
            }
        }

        Row {
            spacing: 8
            Label { text: qsTr("Multiple Connections Penalty:"); width: 180 }
            TextField {
                id: multipleConnectionsPenaltyField
                width: 100
                text: root.pathMachine.multipleConnectionsPenalty.toFixed(4)
                validator: DoubleValidator { bottom: 0; top: 100; decimals: 4 }
                onEditingFinished: root.pathMachine.multipleConnectionsPenalty = parseFloat(text)
            }
        }
    }
}
