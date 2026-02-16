// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#include "AudioManager.h"

#include "../configuration/configuration.h"
#include "../global/Charset.h"
#include "../map/mmapper2room.h"

#ifndef MMAPPER_NO_AUDIO
#include <QAudioOutput>
#include <QMediaPlayer>
#include <QSoundEffect>
#endif
#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <QRegularExpression>
#include <QTimer>
#include <QUrl>

AudioManager::AudioManager(GameObserver &observer, QObject *parent)
    : QObject(parent)
    , m_observer(observer)
{
#ifndef MMAPPER_NO_AUDIO
    m_music.player = new QMediaPlayer(this);
    m_music.audioOutput = new QAudioOutput(this);
    m_music.player->setAudioOutput(m_music.audioOutput);
    m_music.player->setLoops(QMediaPlayer::Infinite);

    connect(m_music.player,
            &QMediaPlayer::mediaStatusChanged,
            this,
            [this](QMediaPlayer::MediaStatus status) {
                if (status == QMediaPlayer::LoadedMedia || status == QMediaPlayer::BufferedMedia
                    || status == QMediaPlayer::BufferingMedia) {
                    applyPendingPosition();
                }
            });

    connect(m_music.player, &QMediaPlayer::seekableChanged, this, [this](bool seekable) {
        if (seekable) {
            applyPendingPosition();
        }
    });

    m_fade.timer = new QTimer(this);
    m_fade.timer->setInterval(100);
    connect(m_fade.timer, &QTimer::timeout, this, [this]() {
        float currentVol = m_music.audioOutput->volume();
        float target = static_cast<float>(getConfig().audio.musicVolume) / 100.0f;

        if (m_fade.isFadingOut) {
            currentVol -= m_fade.step;
            if (currentVol <= 0.0f) {
                currentVol = 0.0f;
                m_fade.timer->stop();

                if (!m_music.currentFile.isEmpty()
                    && m_music.player->playbackState() == QMediaPlayer::PlayingState) {
                    m_music.cachedPositions.insert(m_music.currentFile,
                                                   new qint64(m_music.player->position()));
                }

                m_music.player->stop();
                if (!m_music.pendingFile.isEmpty()) {
                    m_music.currentFile = m_music.pendingFile;
                    m_music.pendingFile.clear();

                    if (qint64 *pos = m_music.cachedPositions.object(m_music.currentFile)) {
                        m_music.pendingPosition = *pos;
                    } else {
                        m_music.pendingPosition = -1;
                    }

                    if (m_music.currentFile.startsWith(":")) {
                        m_music.player->setSource(QUrl("qrc" + m_music.currentFile));
                    } else {
                        m_music.player->setSource(QUrl::fromLocalFile(m_music.currentFile));
                    }

                    if (getConfig().audio.musicVolume > 0) {
                        m_music.player->play();
                        applyPendingPosition();
                        startFadeIn();
                    }
                }
            }
        } else {
            currentVol += m_fade.step;
            if (currentVol >= target) {
                currentVol = target;
                m_fade.timer->stop();
            }
        }
        m_music.audioOutput->setVolume(currentVol);
    });

    scanDirectories();

    const auto &resourcesDir = getConfig().canvas.resourcesDirectory;
    m_resources.watcher.addPath(resourcesDir + "/music");
    m_resources.watcher.addPath(resourcesDir + "/sounds");
    connect(&m_resources.watcher,
            &QFileSystemWatcher::directoryChanged,
            this,
            [this](const QString & /*path*/) { scanDirectories(); });
#endif

    m_observer.sig2_areaChanged.connect(m_lifetime,
                                        [this](const RoomArea &area) { onAreaChanged(area); });
    m_observer.sig2_gainedLevel.connect(m_lifetime, [this]() { onGainedLevel(); });
    m_observer.sig2_positionChanged.connect(m_lifetime, [this](CharacterPositionEnum pos) {
        onPositionChanged(pos);
    });

    updateVolumes();
}

AudioManager::~AudioManager()
{
#ifndef MMAPPER_NO_AUDIO
    m_music.player->stop();
#endif
}

void AudioManager::onAreaChanged(const RoomArea &area)
{
#ifndef MMAPPER_NO_AUDIO
    if (area.isEmpty()) {
        m_music.pendingFile.clear();
        startFadeOut();
        return;
    }

    static const QRegularExpression regex("^the\\s+");
    QString name = area.toQString().toLower().remove(regex).replace(' ', '-');
    mmqt::toAsciiInPlace(name);

    QString musicFile = findAudioFile("music", name);

    if (musicFile.isEmpty()) {
        startFadeOut();
        return;
    }

    if (musicFile == m_music.currentFile) {
        if (m_fade.isFadingOut) {
            m_music.pendingFile.clear();
            startFadeIn();
        } else if (m_music.player->playbackState() == QMediaPlayer::StoppedState
                   && getConfig().audio.musicVolume > 0) {
            if (qint64 *pos = m_music.cachedPositions.object(m_music.currentFile)) {
                m_music.pendingPosition = *pos;
            }
            m_music.player->play();
            applyPendingPosition();
            startFadeIn();
        }
        return;
    }

    if (musicFile == m_music.pendingFile) {
        return;
    }

    m_music.pendingFile = musicFile;
    startFadeOut();
#else
    Q_UNUSED(area);
#endif
}

void AudioManager::onGainedLevel()
{
    playSound("level_up");
}

void AudioManager::onPositionChanged(CharacterPositionEnum position)
{
    if (position == CharacterPositionEnum::FIGHTING) {
        playSound("combat_start");
    }
}

void AudioManager::updateVolumes()
{
#ifndef MMAPPER_NO_AUDIO
    if (!m_fade.isFadingOut && (m_fade.timer == nullptr || !m_fade.timer->isActive())) {
        float vol = static_cast<float>(getConfig().audio.musicVolume) / 100.0f;
        m_music.audioOutput->setVolume(vol);
        if (vol > 0 && m_music.player->playbackState() == QMediaPlayer::StoppedState
            && !m_music.currentFile.isEmpty()) {
            if (qint64 *pos = m_music.cachedPositions.object(m_music.currentFile)) {
                m_music.pendingPosition = *pos;
            } else {
                m_music.pendingPosition = -1;
            }
            m_music.player->play();
            applyPendingPosition();
        } else if (vol <= 0 && m_music.player->playbackState() == QMediaPlayer::PlayingState) {
            if (!m_music.currentFile.isEmpty()) {
                m_music.cachedPositions.insert(m_music.currentFile,
                                               new qint64(m_music.player->position()));
            }
            m_music.player->stop();
        }
    }
#endif
}

#ifndef MMAPPER_NO_AUDIO
void AudioManager::applyPendingPosition()
{
    if (m_music.pendingPosition != -1 && m_music.player->isSeekable()) {
        m_music.player->setPosition(m_music.pendingPosition);
        m_music.pendingPosition = -1;
    }
}
#endif

void AudioManager::playMusic(const QString &areaName)
{
#ifndef MMAPPER_NO_AUDIO
    QString musicFile = findAudioFile("music", areaName);
    if (musicFile.isEmpty())
        return;

    m_music.pendingFile = musicFile;
    startFadeOut();
#else
    Q_UNUSED(areaName);
#endif
}

void AudioManager::playSound(const QString &soundName)
{
    Q_UNUSED(soundName);
#ifndef MMAPPER_NO_AUDIO
    if (getConfig().audio.soundVolume <= 0) {
        return;
    }

    QString name = soundName.toLower().replace(' ', '-');
    QString soundFile = findAudioFile("sounds", name);
    if (soundFile.isEmpty()) {
        return;
    }

    QSoundEffect *effect = new QSoundEffect(this);
    if (soundFile.startsWith(":")) {
        effect->setSource(QUrl("qrc" + soundFile));
    } else {
        effect->setSource(QUrl::fromLocalFile(soundFile));
    }
    effect->setVolume(static_cast<float>(getConfig().audio.soundVolume) / 100.0f);

    connect(effect, &QSoundEffect::loadedChanged, this, [effect]() {
        if (effect->isLoaded()) {
            effect->play();
        }
    });

    // Cleanup when done
    connect(effect, &QSoundEffect::playingChanged, this, [effect]() {
        if (!effect->isPlaying()) {
            effect->deleteLater();
        }
    });
#endif
}

QString AudioManager::findAudioFile(const QString &subDir, const QString &name)
{
    QString key = subDir + "/" + name;
    auto it = m_resources.availableFiles.find(key);
    if (it != m_resources.availableFiles.end()) {
        return it.value();
    }
    return "";
}

void AudioManager::scanDirectories()
{
    m_resources.availableFiles.clear();
    QStringList extensions = {"mp3", "wav", "ogg", "m4a"};

    auto scanPath = [&](const QString &path) {
        QDirIterator it(path, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            QFileInfo fileInfo(it.next());
            QString suffix = fileInfo.suffix().toLower();

            if (extensions.contains(suffix)) {
                QString filePath = fileInfo.filePath();
                auto dotIndex = filePath.lastIndexOf('.');
                if (dotIndex == -1) {
                    continue;
                }

                const bool isQrc = filePath.startsWith(":/");
                QString baseName;
                if (isQrc) {
                    baseName = filePath.left(dotIndex).mid(2); // remove :/
                } else {
                    QString resourcesPath = getConfig().canvas.resourcesDirectory;
                    if (!filePath.startsWith(resourcesPath)) {
                        continue;
                    }
                    baseName = filePath.mid(resourcesPath.length())
                                   .left(dotIndex - resourcesPath.length());
                    if (baseName.startsWith("/")) {
                        baseName = baseName.mid(1);
                    }
                }

                if (baseName.isEmpty()) {
                    continue;
                }

                m_resources.availableFiles.insert(baseName, filePath);
            }
        }
    };

    // Scan QRC first then disk to prioritize disk
    scanPath(":/music");
    scanPath(":/sounds");

    const auto &resourcesDir = getConfig().canvas.resourcesDirectory;
    scanPath(resourcesDir + "/music");
    scanPath(resourcesDir + "/sounds");

    qInfo() << "Scanned audio directories. Found" << m_resources.availableFiles.size() << "files.";
}

void AudioManager::startFadeOut()
{
#ifndef MMAPPER_NO_AUDIO
    m_fade.isFadingOut = true;
    if (m_fade.timer && !m_fade.timer->isActive()) {
        m_fade.timer->start();
    }
#endif
}

void AudioManager::startFadeIn()
{
#ifndef MMAPPER_NO_AUDIO
    m_fade.isFadingOut = false;
    if (m_fade.timer && !m_fade.timer->isActive()) {
        m_fade.timer->start();
    }
#endif
}
