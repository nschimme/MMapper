// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

import QtQuick
import QtQuick.Controls

import MMapper

// Graphics preferences page, driven by preferencesController.graphics (a
// GraphicsPageAdapter, see ../preferences/GraphicsPageAdapter.h). Mirrors
// graphicspage.ui's OpenGL / Room Details / Mapping Hints / Colors /
// Weather / Advanced Settings group boxes without a .ui file. Color swatches
// are plain Rectangles (mirroring the widget page's icon-on-button swatch)
// since there is no packaged-module color-picker button; the native
// QColorDialog is opened via the adapter's chooseX() invokables.
Column {
    id: root
    spacing: 8

    readonly property var graphics: preferencesController.graphics

    component ColorRow: Row {
        property alias label: rowLabel.text
        property color swatchColor
        signal chooseRequested()
        spacing: 8
        Label { id: rowLabel; width: 160 }
        Rectangle {
            width: 24
            height: 16
            border.color: "black"
            border.width: 1
            color: swatchColor
            anchors.verticalCenter: parent.verticalCenter
        }
        Button {
            text: qsTr("Select")
            onClicked: chooseRequested()
        }
    }

    Label { text: qsTr("OpenGL"); font.bold: true }

    Row {
        spacing: 8
        Label { text: qsTr("Anti-aliasing samples:"); width: 160 }
        ComboBox {
            id: aaCombo
            width: 120
            model: root.graphics.antialiasingSamplesLabels
            currentIndex: root.graphics.antialiasingSamplesIndex
            onActivated: index => root.graphics.antialiasingSamplesIndex = index
        }
    }

    CheckBox {
        text: qsTr("Trilinear filtering")
        checked: root.graphics.trilinearFiltering
        onToggled: root.graphics.trilinearFiltering = checked
    }

    Label { text: qsTr("Room Details"); font.bold: true }

    CheckBox {
        text: qsTr("Texture upper layers")
        checked: root.graphics.drawUpperLayersTextured
        onToggled: root.graphics.drawUpperLayersTextured = checked
    }

    CheckBox {
        text: qsTr("Render hidden doors")
        checked: root.graphics.drawDoorNames
        onToggled: root.graphics.drawDoorNames = checked
    }

    Label { text: qsTr("Mapping Hints"); font.bold: true }

    CheckBox {
        text: qsTr("Show unsaved changes")
        checked: root.graphics.showUnsavedChanges
        onToggled: root.graphics.showUnsavedChanges = checked
    }

    CheckBox {
        text: qsTr("Show missing map ids")
        checked: root.graphics.showMissingMapId
        onToggled: root.graphics.showMissingMapId = checked
    }

    CheckBox {
        text: qsTr("Show unmapped exits")
        checked: root.graphics.showUnmappedExits
        onToggled: root.graphics.showUnmappedExits = checked
    }

    Label { text: qsTr("Colors"); font.bold: true }

    ColorRow {
        label: qsTr("Background:")
        swatchColor: root.graphics.backgroundColor
        onChooseRequested: root.graphics.chooseBackgroundColor()
    }
    ColorRow {
        label: qsTr("Dark w/o Sundeath:")
        swatchColor: root.graphics.roomDarkColor
        onChooseRequested: root.graphics.chooseRoomDarkColor()
    }
    ColorRow {
        label: qsTr("Light w/o Sundeath:")
        swatchColor: root.graphics.roomDarkLitColor
        onChooseRequested: root.graphics.chooseRoomDarkLitColor()
    }
    ColorRow {
        label: qsTr("Normal Connection:")
        swatchColor: root.graphics.connectionNormalColor
        onChooseRequested: root.graphics.chooseConnectionNormalColor()
    }

    Label { text: qsTr("Weather and Atmosphere"); font.bold: true }

    Row {
        spacing: 8
        Label { text: qsTr("Atmosphere Intensity:"); width: 160 }
        Slider {
            from: 0
            to: 100
            stepSize: 1
            value: root.graphics.weatherAtmosphereIntensity
            onMoved: root.graphics.weatherAtmosphereIntensity = value
        }
    }
    Row {
        spacing: 8
        Label { text: qsTr("Precipitation Intensity:"); width: 160 }
        Slider {
            from: 0
            to: 100
            stepSize: 1
            value: root.graphics.weatherPrecipitationIntensity
            onMoved: root.graphics.weatherPrecipitationIntensity = value
        }
    }
    Row {
        spacing: 8
        Label { text: qsTr("Time of Day Intensity:"); width: 160 }
        Slider {
            from: 0
            to: 100
            stepSize: 1
            value: root.graphics.weatherTimeOfDayIntensity
            onMoved: root.graphics.weatherTimeOfDayIntensity = value
        }
    }

    Label { text: qsTr("Advanced Settings"); font.bold: true }

    CheckBox {
        text: qsTr("Show Performance Stats")
        checked: root.graphics.showPerfStats
        onToggled: root.graphics.showPerfStats = checked
    }
    CheckBox {
        id: use3dCheckBox
        text: qsTr("3d Mode")
        checked: root.graphics.use3D
        onToggled: root.graphics.use3D = checked
    }
    CheckBox {
        text: qsTr("Auto tilt with zoom")
        enabled: use3dCheckBox.checked
        checked: root.graphics.autoTilt
        onToggled: root.graphics.autoTilt = checked
    }

    Repeater {
        model: root.graphics.advancedModel
        delegate: Column {
            width: root.width
            spacing: 2
            enabled: model.is3DOnly ? use3dCheckBox.checked : true
            Label { text: model.name }
            Row {
                spacing: 8
                Slider {
                    id: advSlider
                    width: 260
                    from: model.min
                    to: model.max
                    stepSize: 1
                    value: model.value
                    onMoved: root.graphics.advancedModel.setValue(index, value)
                }
                Label {
                    anchors.verticalCenter: advSlider.verticalCenter
                    text: model.displayValue.toFixed(model.digits)
                }
                Button {
                    text: qsTr("Reset")
                    onClicked: root.graphics.advancedModel.reset(index)
                }
            }
        }
    }
}
