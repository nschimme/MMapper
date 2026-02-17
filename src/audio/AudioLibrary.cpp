// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "AudioLibrary.h"

#include "../configuration/configuration.h"

#ifndef MMAPPER_NO_AUDIO
#include <QMediaFormat>
#include <QMimeType>
#endif

#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <QtGlobal>

AudioLibrary::AudioLibrary(QObject *const parent)
    : QObject(parent)
{
    scanDirectories();

    const auto &resourcesDir = getConfig().canvas.resourcesDirectory;
    m_watcher.addPath(resourcesDir + "/music");
    m_watcher.addPath(resourcesDir + "/sounds");
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, [this](const QString &) {
        scanDirectories();
    });
}

QString AudioLibrary::findAudioFile(const QString &subDir, const QString &name) const
{
    QString key = subDir + "/" + name;
    auto it = m_availableFiles.find(key);
    if (it != m_availableFiles.end()) {
        return it.value();
    }
    return "";
}

void AudioLibrary::scanDirectories()
{
    m_availableFiles.clear();
    QStringList extensions;

#ifndef MMAPPER_NO_AUDIO
    QMediaFormat format;
    for (auto f : format.supportedFileFormats(QMediaFormat::Decode)) {
        QMediaFormat info(f);
        QMimeType mime = info.mimeType();
        if (mime.isValid() && mime.name().startsWith(u"audio/")) {
            for (const QString &suffix : mime.suffixes()) {
                QString s = suffix.toLower();
                if (!extensions.contains(s)) {
                    extensions.append(s);
                }
            }
        }
    }
    if constexpr (CURRENT_PLATFORM == PlatformEnum::Mac) {
        // Mac backend supports mp3 but does not list it
        extensions.append("mp3");
    }
#endif
    qInfo() << "Supported Audio Formats:" << extensions;

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

                const bool isQrc = filePath.startsWith(QLatin1String(":/"));
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
                    if (baseName.startsWith(QLatin1String("/"))) {
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
    scanPath(QLatin1String(":/music"));
    scanPath(QLatin1String(":/sounds"));

    const auto &resourcesDir = getConfig().canvas.resourcesDirectory;
    scanPath(resourcesDir + "/music");
    scanPath(resourcesDir + "/sounds");

    qInfo() << "Scanned audio directories. Found" << m_availableFiles.size() << "files.";
}
