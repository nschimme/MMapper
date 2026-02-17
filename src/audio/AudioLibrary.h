#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "../global/macros.h"

#include <QFileSystemWatcher>
#include <QMap>
#include <QObject>
#include <QString>

class NODISCARD_QOBJECT AudioLibrary final : public QObject
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
