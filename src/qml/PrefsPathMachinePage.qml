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
// valueFromText), each double field is the DoubleField inline component
// below: a TextField constrained by DoubleValidator(0, 100, 4) (matching
// acceptBestRelativeDoubleSpinBox et al.'s 0..100 range / 4 decimals in
// pathmachinepage.ui) plus −/+ stepper buttons (a non-keyboard input path
// for touch), committing the clamped value to the adapter.
Column {
    id: root
    spacing: 8

    readonly property var pathMachine: preferencesController.pathMachine

    // A 0..100 double field with −/+ steppers (a non-keyboard input path for
    // touch, which the bare TextField+DoubleValidator lacked) plus direct
    // typing. Step 1.0 matches the widget's QDoubleSpinBox default; the
    // steppers and typing both clamp to [0,100] via commit().
    component DoubleField: Row {
        id: df
        spacing: 8
        property string label
        property real value
        property real step: 1.0
        signal committed(real v)

        function commit(v) {
            if (!isNaN(v))
                df.committed(Math.max(0, Math.min(100, v)));
        }

        Label {
            text: df.label
            width: 180
            anchors.verticalCenter: parent.verticalCenter
        }
        Button {
            text: "−"
            implicitHeight: Theme.controlHeight
            implicitWidth: Theme.controlHeight
            onClicked: df.commit(df.value - df.step)
        }
        TextField {
            width: 100
            text: df.value.toFixed(4)
            horizontalAlignment: TextInput.AlignRight
            inputMethodHints: Qt.ImhFormattedNumbersOnly
            validator: DoubleValidator { bottom: 0; top: 100; decimals: 4 }
            onEditingFinished: df.commit(parseFloat(text))
        }
        Button {
            text: "+"
            implicitHeight: Theme.controlHeight
            implicitWidth: Theme.controlHeight
            onClicked: df.commit(df.value + df.step)
        }
    }

    Label { text: qsTr("WARNING: These settings are for advanced users only"); font.bold: true }

    Row {
        spacing: 8
        Label { text: qsTr("Max Paths:"); width: 180 }
        SpinBox {
            id: maxPathsBox
            editable: true
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
            editable: true
            from: 0
            to: 100
            value: root.pathMachine.matchingTolerance
            onValueModified: root.pathMachine.matchingTolerance = value
        }
    }

    DoubleField {
        label: qsTr("Accept Best Relative:")
        value: root.pathMachine.acceptBestRelative
        onCommitted: root.pathMachine.acceptBestRelative = v
    }

    DoubleField {
        label: qsTr("Accept Best Absolute:")
        value: root.pathMachine.acceptBestAbsolute
        onCommitted: root.pathMachine.acceptBestAbsolute = v
    }

    DoubleField {
        label: qsTr("New Room Penalty:")
        value: root.pathMachine.newRoomPenalty
        onCommitted: root.pathMachine.newRoomPenalty = v
    }

    DoubleField {
        label: qsTr("Correct Position bonus:")
        value: root.pathMachine.correctPositionBonus
        onCommitted: root.pathMachine.correctPositionBonus = v
    }

    DoubleField {
        label: qsTr("Multiple Connections Penalty:")
        value: root.pathMachine.multipleConnectionsPenalty
        onCommitted: root.pathMachine.multipleConnectionsPenalty = v
    }
}
