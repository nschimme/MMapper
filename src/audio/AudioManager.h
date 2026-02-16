#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#include "../observer/gameobserver.h"

#include <memory>

#include <QCache>
#include <QFileSystemWatcher>
#include <QMap>
#include <QObject>
#include <QString>

#ifndef MMAPPER_NO_AUDIO
class QMediaPlayer;
class QAudioOutput;
#endif
class QTimer;

class NODISCARD_QOBJECT AudioManager final : public QObject
{
    Q_OBJECT

private:
#ifndef MMAPPER_NO_AUDIO
    struct MusicChannel
    {
        QMediaPlayer *player = nullptr;
        QAudioOutput *audioOutput = nullptr;
        QString file;
        qint64 pendingPosition = -1;
        float fadeVolume = 0.0f;
    };

    struct MusicState
    {
        MusicChannel channels[2];
        int activeChannel = 0;
        QCache<QString, qint64> cachedPositions;

        MusicState() { cachedPositions.setMaxCost(10); }
    } m_music;
#endif

    struct FadeState
    {
        QTimer *timer = nullptr;
        float step = 0.05f;
        bool fadingToSilence = false;
        static constexpr int CROSSFADE_DURATION_MS = 2000;
        static constexpr int FADE_INTERVAL_MS = 100;
    } m_fade;

    struct ResourceState
    {
        QFileSystemWatcher watcher;
        QMap<QString, QString> availableFiles;
    } m_resources;

    GameObserver &m_observer;
    Signal2Lifetime m_lifetime;

public:
    explicit AudioManager(GameObserver &observer, QObject *parent = nullptr);
    ~AudioManager() override;

public slots:
    void onAreaChanged(const RoomArea &area);
    void onGainedLevel();
    void onPositionChanged(CharacterPositionEnum position);

    void updateVolumes();

private:
    void playMusic(const QString &areaName);
    void playSound(const QString &soundName);
    QString findAudioFile(const QString &subDir, const QString &name);
    void scanDirectories();

    void startFade(bool toSilence);
#ifndef MMAPPER_NO_AUDIO
    void applyPendingPosition(int channelIndex);
    void updateChannelVolume(int channelIndex);
#endif
};
