#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "../global/Signal2.h"
#include "../global/macros.h"

#include <QByteArray>
#include <QList>
#include <QObject>
#include <QStringList>

#ifndef MMAPPER_NO_AUDIO
class QMediaDevices;
#endif

// Q_PROPERTY façade over Configuration::AudioSettings (see
// ../configuration/configuration.h), mirroring audiopage.cpp /
// AudioVolumeSlider's apply-on-change semantics. Unlike the other four
// SIMPLE-page adapters, AudioSettings *does* have a ChangeMonitor, so this
// adapter registers a callback (mirroring AudioVolumeSlider::init()) and
// stays live without needing an explicit reload() from the dialog shell —
// reload() is still provided for symmetry with the other adapters and to
// force a resync of the device list.
//
// enabled mirrors the NO_AUDIO build-time toggle (see
// ../global/ConfigConsts.h): when true, the audio backend is compiled out
// and the page should disable itself entirely, same as
// AudioPage::outputDeviceComboBox->setEnabled(false) /
// AudioVolumeSlider::setEnabled(false).
//
// deviceNames is a flat display-name list ("System Default" first, then any
// QMediaDevices::audioOutputs(), then a "<id> (missing)" entry if the
// configured device vanished — mirrors AudioPage::slot_updateDevices()).
// selectedDeviceIndex indexes into that same list; the underlying device ids
// are tracked in parallel (not exposed to QML) and only selectedDeviceIndex's
// setter writes back to Configuration::AudioSettings::setOutputDeviceId().
class NODISCARD_QOBJECT AudioPageAdapter final : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool enabled READ getEnabled CONSTANT)
    Q_PROPERTY(QStringList deviceNames READ getDeviceNames NOTIFY sig_devicesChanged)
    Q_PROPERTY(int selectedDeviceIndex READ getSelectedDeviceIndex WRITE setSelectedDeviceIndex
                   NOTIFY sig_changed)
    Q_PROPERTY(int musicVolume READ getMusicVolume WRITE setMusicVolume NOTIFY sig_changed)
    Q_PROPERTY(int soundVolume READ getSoundVolume WRITE setSoundVolume NOTIFY sig_changed)

private:
    Signal2Lifetime m_lifetime;
    QStringList m_deviceNames;
    QList<QByteArray> m_deviceIds;
#ifndef MMAPPER_NO_AUDIO
    QMediaDevices *m_mediaDevices = nullptr;
#endif

public:
    explicit AudioPageAdapter(QObject *parent);

public:
    NODISCARD static bool getEnabled();

    NODISCARD QStringList getDeviceNames() const { return m_deviceNames; }

    NODISCARD int getSelectedDeviceIndex() const;
    void setSelectedDeviceIndex(int index);

    NODISCARD int getMusicVolume() const;
    void setMusicVolume(int value);

    NODISCARD int getSoundVolume() const;
    void setSoundVolume(int value);

public slots:
    // Rebuilds deviceNames/the parallel id list from the live
    // QMediaDevices::audioOutputs() (mirrors
    // AudioPage::slot_updateDevices()) and emits sig_devicesChanged.
    void refreshDevices();

    // Re-emits sig_changed (for the volume/selection properties) and
    // refreshes the device list; must be called whenever code outside this
    // class may have written to getConfig().audio, though the ChangeMonitor
    // callback normally makes this unnecessary for volume changes.
    void reload();

signals:
    void sig_changed();
    void sig_devicesChanged();
};
