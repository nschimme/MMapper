// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#include "AudioManager.h"

#include "../configuration/configuration.h"
#include "../global/Charset.h"
#ifdef WITH_AUDIO
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
#ifdef WITH_AUDIO
    m_player = new QMediaPlayer(this);
    m_audioOutput = new QAudioOutput(this);
    m_player->setAudioOutput(m_audioOutput);
    m_player->setLoops(QMediaPlayer::Infinite);

    m_fadeTimer = new QTimer(this);
    m_fadeTimer->setInterval(100);
    connect(m_fadeTimer, &QTimer::timeout, this, [this]() {
        float currentVol = m_audioOutput->volume();
        float target = static_cast<float>(getConfig().audio.musicVolume) / 100.0f;
        if (!getConfig().audio.musicEnabled) {
            target = 0.0f;
        }

        if (m_isFadingOut) {
            currentVol -= m_fadeStep;
            if (currentVol <= 0.0f) {
                currentVol = 0.0f;
                m_fadeTimer->stop();
                m_player->stop();
                if (!m_pendingMusicFile.isEmpty()) {
                    m_currentMusicFile = m_pendingMusicFile;
                    m_pendingMusicFile.clear();
                    if (m_currentMusicFile.startsWith(":")) {
                        m_player->setSource(QUrl("qrc" + m_currentMusicFile));
                    } else {
                        m_player->setSource(QUrl::fromLocalFile(m_currentMusicFile));
                    }
                    if (getConfig().audio.musicEnabled) {
                        m_player->play();
                        startFadeIn();
                    }
                }
            }
        } else {
            currentVol += m_fadeStep;
            if (currentVol >= target) {
                currentVol = target;
                m_fadeTimer->stop();
            }
        }
        m_audioOutput->setVolume(currentVol);
    });

    scanDirectories();

    const auto &resourcesDir = getConfig().canvas.resourcesDirectory;
    m_watcher.addPath(resourcesDir + "/music");
    m_watcher.addPath(resourcesDir + "/sounds");
    connect(&m_watcher,
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
#ifdef WITH_AUDIO
    m_player->stop();
#endif
}

void AudioManager::onAreaChanged(const RoomArea &area)
{
    if (area.isEmpty()) {
        m_pendingMusicFile.clear();
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

    if (musicFile == m_currentMusicFile) {
        return;
    }

    m_pendingMusicFile = musicFile;
    startFadeOut();
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
#ifdef WITH_AUDIO
    if (!m_isFadingOut && (m_fadeTimer == nullptr || !m_fadeTimer->isActive())) {
        float vol = getConfig().audio.musicEnabled
                        ? static_cast<float>(getConfig().audio.musicVolume) / 100.0f
                        : 0.0f;
        m_audioOutput->setVolume(vol);
    }
#endif
}

void AudioManager::playMusic(const QString &areaName)
{
    QString musicFile = findAudioFile("music", areaName);
    if (musicFile.isEmpty())
        return;

    m_pendingMusicFile = musicFile;
    startFadeOut();
}

void AudioManager::playSound(const QString &soundName)
{
#ifdef WITH_AUDIO
    if (!getConfig().audio.soundsEnabled) {
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
#else
    Q_UNUSED(soundName);
#endif
}

QString AudioManager::findAudioFile(const QString &subDir, const QString &name)
{
    QString key = subDir + "/" + name;
    auto it = m_availableFiles.find(key);
    if (it != m_availableFiles.end()) {
        return it.value();
    }
    return "";
}

void AudioManager::scanDirectories()
{
    m_availableFiles.clear();
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

                m_availableFiles.insert(baseName, filePath);
            }
        }
    };

    // Scan QRC first then disk to prioritize disk
    scanPath(":/music");
    scanPath(":/sounds");

    const auto &resourcesDir = getConfig().canvas.resourcesDirectory;
    scanPath(resourcesDir + "/music");
    scanPath(resourcesDir + "/sounds");

    qInfo() << "Scanned audio directories. Found" << m_availableFiles.size() << "files.";
}

void AudioManager::startFadeOut()
{
#ifdef WITH_AUDIO
    m_isFadingOut = true;
    if (m_fadeTimer && !m_fadeTimer->isActive()) {
        m_fadeTimer->start();
    }
#endif
}

void AudioManager::startFadeIn()
{
#ifdef WITH_AUDIO
    m_isFadingOut = false;
    if (m_fadeTimer && !m_fadeTimer->isActive()) {
        m_fadeTimer->start();
    }
#endif
}
