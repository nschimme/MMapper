#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#include <QObject>
#include <QString>

class AudioLibrary;

class SfxManager final : public QObject
{
    Q_OBJECT

private:
    const AudioLibrary &m_library;

public:
    explicit SfxManager(const AudioLibrary &library, QObject *parent = nullptr);

    void playSound(const QString &soundName);
    void updateVolume();
};
