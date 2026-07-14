// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

import QtQuick
import QtQuick.Controls

import MMapper

// Audio preferences page, driven by preferencesController.audio (an
// AudioPageAdapter, see ../preferences/AudioPageAdapter.h). Mirrors
// audiopage.ui's output-device combo box and the two AudioVolumeSlider
// sliders without a .ui file. The whole page disables itself when
// audio.enabled is false (NO_AUDIO build).
Flickable {
    id: root
    clip: true
    contentWidth: width
    contentHeight: column.implicitHeight
    enabled: audio.enabled

    readonly property var audio: preferencesController.audio

    Column {
        id: column
        width: root.width
        spacing: 8

        Row {
            spacing: 8
            Label { text: qsTr("Output device:"); width: 120 }
            ComboBox {
                id: outputDeviceCombo
                width: 260
                model: root.audio.deviceNames
                currentIndex: root.audio.selectedDeviceIndex
                onActivated: index => root.audio.selectedDeviceIndex = index
            }
        }

        Row {
            spacing: 8
            Label { text: qsTr("Music volume:"); width: 120 }
            Slider {
                id: musicVolumeSlider
                from: 0
                to: 100
                stepSize: 1
                value: root.audio.musicVolume
                onMoved: root.audio.musicVolume = value
            }
        }

        Row {
            spacing: 8
            Label { text: qsTr("Sound volume:"); width: 120 }
            Slider {
                id: soundVolumeSlider
                from: 0
                to: 100
                stepSize: 1
                value: root.audio.soundVolume
                onMoved: root.audio.soundVolume = value
            }
        }
    }
}
