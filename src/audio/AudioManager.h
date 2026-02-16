#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#include "../observer/gameobserver.h"
#include <QObject>
#include <QString>
#include <memory>

#ifdef MMAPPER_WITH_MULTIMEDIA
class QMediaPlayer;
class QAudioOutput;
#endif
class QTimer;

class NODISCARD_QOBJECT AudioManager final : public QObject
{
    Q_OBJECT

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

    void startFadeOut();
    void startFadeIn();

private:
    GameObserver &m_observer;
#ifdef MMAPPER_WITH_MULTIMEDIA
    QMediaPlayer *m_player = nullptr;
    QAudioOutput *m_audioOutput = nullptr;
#endif
    QTimer *m_fadeTimer = nullptr;

    QString m_currentMusicFile;
    QString m_pendingMusicFile;
    float m_fadeStep = 0.05f;
    bool m_isFadingOut = false;

    Signal2Lifetime m_lifetime;
};
