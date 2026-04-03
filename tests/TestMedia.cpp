// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "TestMedia.h"

#include "../src/configuration/configuration.h"
#include "../src/display/Filenames.h"
#include "../src/media/AudioManager.h"

QString getAssetsPath()
{
    return "assets/";
}
#include "../src/media/MediaLibrary.h"
#include "../src/media/MusicManager.h"
#include "../src/media/SfxManager.h"
#include "../src/observer/gameobserver.h"

#include <QtTest/QtTest>

void TestMedia::initTestCase()
{
    setEnteredMain();
}

void TestMedia::musicManagerStopTest()
{
#ifndef MMAPPER_NO_AUDIO
    MediaLibrary library;
    MusicManager manager(library);

    // standard stop (fade)
    manager.stopMusic(false);

    // immediate stop
    manager.stopMusic(true);
#endif
}

void TestMedia::sfxManagerStopTest()
{
#ifndef MMAPPER_NO_AUDIO
    MediaLibrary library;
    SfxManager manager(library);

    // immediate stop
    manager.stopAll(true);

    // non-immediate stop (no-op in current implementation but test for coverage)
    manager.stopAll(false);
#endif
}

void TestMedia::audioManagerStopTest()
{
#ifndef MMAPPER_NO_AUDIO
    MediaLibrary library;
    GameObserver observer;
    AudioManager manager(library, observer);

    // immediate stop
    manager.stop(true);

    // standard stop
    manager.stop(false);
#endif
}

void TestMedia::cleanupTestCase() {}

QTEST_MAIN(TestMedia)
