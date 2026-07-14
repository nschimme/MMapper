// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "TestMainWindow.h"

#include "../src/configuration/configuration.h"
#include "../src/global/Version.h"
#include "../src/mainwindow/AudioVolumeSlider.h"
#include "../src/mainwindow/UpdateChecker.h"
#include "../src/preferences/GroupPageAdapter.h"
#include "../src/preferences/PathMachinePageAdapter.h"
#include "../src/preferences/PreferencesController.h"

#include <QDebug>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QtTest/QtTest>

const char *getMMapperVersion()
{
    return "v19.04.0-72-ga16c196";
}

const char *getMMapperBranch()
{
    return "master";
}

bool isMMapperBeta()
{
    return false;
}

TestMainWindow::TestMainWindow() = default;

TestMainWindow::~TestMainWindow() = default;

void TestMainWindow::updaterTest()
{
    CompareVersion version{QString::fromUtf8(getMMapperVersion())};
    QVERIFY2(version == version, "Version compared to itself matches");

    CompareVersion current{"2.8.0"};
    QVERIFY2((current > current) == false, "Current is not greater than itself");

    CompareVersion newerMajor{"19.04.0"};
    QVERIFY2(newerMajor > current, "Newer major version is greater than older version");
    QVERIFY2((current > newerMajor) == false, "Older version is not newer than newer major");

    CompareVersion newerMinor{"2.9.0"};
    QVERIFY2(newerMinor > current, "Newer major version is greater than older version");
    QVERIFY2((current > newerMinor) == false, "Older version is not newer than newer minor version");

    CompareVersion newerPatch{"2.9.0"};
    QVERIFY2(newerPatch > current, "Newer major version is greater than older version");
    QVERIFY2((current > newerPatch) == false, "Older version is not newer than newer patch version");
}

void TestMainWindow::audioToolbarTest()
{
    setEnteredMain();

    const int originalMusic = getConfig().audio.getMusicVolume();
    const int originalSound = getConfig().audio.getSoundVolume();

    // Ensure we restore configuration
    auto cleanup = qScopeGuard([=]() {
        setConfig().audio.setMusicVolume(originalMusic);
        setConfig().audio.setSoundVolume(originalSound);
    });

    AudioVolumeSlider musicSlider(AudioVolumeSlider::AudioType::Music);
    AudioVolumeSlider soundSlider(AudioVolumeSlider::AudioType::Sound);

    // Initial value should match current config defaults
    QCOMPARE(musicSlider.value(), getConfig().audio.getMusicVolume());
    QCOMPARE(soundSlider.value(), getConfig().audio.getSoundVolume());

    // Update config -> slider updates
    setConfig().audio.setMusicVolume(75);
    QCOMPARE(musicSlider.value(), 75);

    setConfig().audio.setSoundVolume(25);
    QCOMPARE(soundSlider.value(), 25);

    // Update slider -> config updates
    musicSlider.setValue(90);
    QCOMPARE(getConfig().audio.getMusicVolume(), 90);

    soundSlider.setValue(10);
    QCOMPARE(getConfig().audio.getSoundVolume(), 10);
}

void TestMainWindow::pathMachinePageAdapterRoundTrip()
{
    setEnteredMain();

    const auto original = getConfig().pathMachine;
    // TestMainWindow mutates the global Configuration singleton, which is
    // shared across all tests in this binary; always restore it, following
    // the precedent in audioToolbarTest() above.
    auto cleanup = qScopeGuard([=]() { setConfig().pathMachine = original; });

    PathMachinePageAdapter adapter(nullptr);
    QSignalSpy changedSpy(&adapter, &PathMachinePageAdapter::sig_changed);

    // setProperty()-driven write must go through the adapter's setter and
    // land in the live Configuration immediately (apply-on-change).
    QVERIFY(adapter.setProperty("maxPaths", 42));
    QCOMPARE(getConfig().pathMachine.maxPaths, 42);
    QCOMPARE(changedSpy.count(), 1);

    QVERIFY(adapter.setProperty("acceptBestRelative", 12.5));
    QCOMPARE(getConfig().pathMachine.acceptBestRelative, 12.5);
    QCOMPARE(changedSpy.count(), 2);

    // Negative ints must clamp to zero, mirroring
    // PathmachinePage::slot_maxPathsValueChanged()'s
    // utils::clampNonNegative() call.
    QVERIFY(adapter.setProperty("matchingTolerance", -5));
    QCOMPARE(getConfig().pathMachine.matchingTolerance, 0);

    // An external writer (e.g. Cancel's setConfig().read()) bypasses the
    // adapter's setters entirely; getters always read live config, so the
    // property reflects the change immediately, but reload() must still be
    // called to notify any bound QML.
    setConfig().pathMachine.matchingTolerance = 7;
    QCOMPARE(adapter.property("matchingTolerance").toInt(), 7);
    adapter.reload();
    QCOMPARE(changedSpy.count(), 4);
}

void TestMainWindow::groupPageAdapterRoundTrip()
{
    setEnteredMain();

    const auto original = getConfig().groupManager;
    auto cleanup = qScopeGuard([=]() { setConfig().groupManager = original; });

    GroupPageAdapter adapter(nullptr, nullptr);
    QSignalSpy changedSpy(&adapter, &GroupPageAdapter::sig_changed);
    QSignalSpy groupSettingsSpy(&adapter, &GroupPageAdapter::sig_groupSettingsChanged);

    // Every setter (npcHide included) must also emit
    // sig_groupSettingsChanged, mirroring GroupPage's per-control
    // emit sig_groupSettingsChanged() calls (see grouppage.cpp) so
    // PreferencesController can forward it the same way ConfigDialog relays
    // GroupPage::sig_groupSettingsChanged.
    QVERIFY(adapter.setProperty("npcHide", true));
    QCOMPARE(getConfig().groupManager.npcHide, true);
    QCOMPARE(changedSpy.count(), 1);
    QCOMPARE(groupSettingsSpy.count(), 1);

    QVERIFY(adapter.setProperty("npcSortBottom", true));
    QCOMPARE(getConfig().groupManager.npcSortBottom, true);
    QCOMPARE(groupSettingsSpy.count(), 2);

    const QColor newColor{Qt::green};
    QVERIFY(adapter.setProperty("color", newColor));
    QCOMPARE(getConfig().groupManager.color, newColor);
    QCOMPARE(groupSettingsSpy.count(), 3);
}

void TestMainWindow::preferencesControllerCancelReloadsAdapters()
{
    setEnteredMain();

    const auto originalPathMachine = getConfig().pathMachine;
    const auto originalGroupManager = getConfig().groupManager;
    auto cleanup = qScopeGuard([=]() {
        setConfig().pathMachine = originalPathMachine;
        setConfig().groupManager = originalGroupManager;
    });

    PreferencesController controller(nullptr, nullptr);
    QSignalSpy groupForwardSpy(&controller, &PreferencesController::sig_groupSettingsChanged);

    // GroupPageAdapter's sig_groupSettingsChanged must be forwarded by the
    // controller (see PreferencesController.cpp's constructor connect),
    // mirroring ConfigDialog's GroupPage -> ConfigDialog relay.
    QVERIFY(controller.getGroup()->setProperty("npcHide", true));
    QCOMPARE(groupForwardSpy.count(), 1);

    // reloadAll() must fan out to every adapter, mirroring
    // ConfigDialog::sig_loadConfig()'s fan-out (see configdialog.cpp).
    QSignalSpy pathMachineChangedSpy(controller.getPathMachine(),
                                     &PathMachinePageAdapter::sig_changed);
    QSignalSpy groupChangedSpy(controller.getGroup(), &GroupPageAdapter::sig_changed);
    controller.reloadAll();
    QCOMPARE(pathMachineChangedSpy.count(), 1);
    QCOMPARE(groupChangedSpy.count(), 1);
}

QTEST_MAIN(TestMainWindow)
