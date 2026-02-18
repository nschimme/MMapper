#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "../global/macros.h"
#include "../observer/gameobserver.h"

#include <QObject>

class MediaLibrary;
class MusicManager;
class SfxManager;

class NODISCARD_QOBJECT AudioManager final : public QObject
{
    Q_OBJECT

private:
    MediaLibrary &m_library;
    MusicManager *m_music = nullptr;
    SfxManager *m_sfx = nullptr;

    GameObserver &m_observer;
    Signal2Lifetime m_lifetime;

    int m_lastMusicVol = 0;
    int m_lastSoundVol = 0;

public:
    explicit AudioManager(MediaLibrary &library, GameObserver &observer, QObject *parent = nullptr);
    ~AudioManager() override;

public:
    void onAreaChanged(const RoomArea &area);
    void playSound(const QString &soundName);
    void unblockAudio();

signals:
    void sig_audioUnblocked();

public slots:
    void slot_updateVolumes();
};
