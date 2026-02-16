#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#include <QFileSystemWatcher>
#include <QMap>
#include <QObject>
#include <QString>

class AudioLibrary final : public QObject
{
    Q_OBJECT

private:
    QFileSystemWatcher m_watcher;
    QMap<QString, QString> m_availableFiles;

public:
    explicit AudioLibrary(QObject *parent = nullptr);

    QString findAudioFile(const QString &subDir, const QString &name) const;

private:
    void scanDirectories();
};
