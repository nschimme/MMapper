#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "../global/macros.h"
#include "../observer/gameobserver.h"

#include <QObject>

class AudioLibrary;
class MusicManager;
class SfxManager;

class NODISCARD_QOBJECT AudioManager final : public QObject
{
    Q_OBJECT

private:
    AudioLibrary *m_library = nullptr;
    MusicManager *m_music = nullptr;
    SfxManager *m_sfx = nullptr;

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
};
