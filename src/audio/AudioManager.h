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
    struct MusicState
    {
        QMediaPlayer *player = nullptr;
        QAudioOutput *audioOutput = nullptr;
        QString currentFile;
        QString pendingFile;
        QCache<QString, qint64> cachedPositions;
        qint64 pendingPosition = -1;

        MusicState() { cachedPositions.setMaxCost(10); }
    } m_music;
#endif

    struct FadeState
    {
        QTimer *timer = nullptr;
        float step = 0.05f;
        bool isFadingOut = false;
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

    void startFadeOut();
    void startFadeIn();
#ifndef MMAPPER_NO_AUDIO
    void applyPendingPosition();
#endif
};
