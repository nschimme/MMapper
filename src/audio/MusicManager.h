#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#include <QCache>
#include <QObject>
#include <QString>

#ifndef MMAPPER_NO_AUDIO
class QMediaPlayer;
class QAudioOutput;
#endif
class QTimer;
class AudioLibrary;

class MusicManager final : public QObject
{
    Q_OBJECT

private:
    const AudioLibrary &m_library;
#ifndef MMAPPER_NO_AUDIO
    struct MusicChannel
    {
        QMediaPlayer *player = nullptr;
        QAudioOutput *audioOutput = nullptr;
        QString file;
        qint64 pendingPosition = -1;
        float fadeVolume = 0.0f;
    };

    void applyPendingPosition(int channelIndex);
    void updateChannelVolume(int channelIndex);
    void startFade(bool toSilence);

    MusicChannel m_channels[2];
    int m_activeChannel = 0;
    QCache<QString, qint64> m_cachedPositions;

    QTimer *m_fadeTimer = nullptr;
    float m_fadeStep = 0.05f;
    bool m_fadingToSilence = false;

    static constexpr int CROSSFADE_DURATION_MS = 2000;
    static constexpr int FADE_INTERVAL_MS = 100;
#endif

public:
    explicit MusicManager(const AudioLibrary &library, QObject *parent = nullptr);
    ~MusicManager() override;

    void playMusic(const QString &musicFile);
    void stopMusic();
    void updateVolumes();
};
