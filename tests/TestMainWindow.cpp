// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "TestMainWindow.h"

#include "../src/configuration/configuration.h"
#include "../src/global/Version.h"
#include "../src/mainwindow/AudioVolumeSlider.h"
#include "../src/mainwindow/UpdateChecker.h"
#include "../src/preferences/AdvancedGraphicsModel.h"
#include "../src/preferences/ClientPageAdapter.h"
#include "../src/preferences/GeneralPageAdapter.h"
#include "../src/preferences/GraphicsPageAdapter.h"
#include "../src/preferences/GroupPageAdapter.h"
#include "../src/preferences/ParserPageAdapter.h"
#include "../src/preferences/PathMachinePageAdapter.h"
#include "../src/preferences/PreferencesController.h"
#include "../src/preferences/ansicombo.h"

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

void TestMainWindow::graphicsPageAdapterRoundTrip()
{
    setEnteredMain();

    // CanvasSettings holds NamedConfig<T>/FixedPoint<D> members and is
    // non-copyable, so restore only the individual fields this test mutates
    // (rather than snapshotting the whole struct like the other adapter
    // tests above).
    const QColor originalBackground = getConfig().canvas.backgroundColor.getColor().getQColor();
    const int originalWeather = getConfig().canvas.weatherAtmosphereIntensity.get();
    const bool originalDrawDoorNames = getConfig().canvas.drawDoorNames;
    auto cleanup = qScopeGuard([=]() {
        setConfig().canvas.backgroundColor = Color(originalBackground);
        setConfig().canvas.weatherAtmosphereIntensity.set(originalWeather);
        setConfig().canvas.drawDoorNames = originalDrawDoorNames;
    });

    GraphicsPageAdapter adapter(nullptr, nullptr);
    QSignalSpy changedSpy(&adapter, &GraphicsPageAdapter::sig_changed);
    QSignalSpy graphicsSettingsSpy(&adapter, &GraphicsPageAdapter::sig_graphicsSettingsChanged);

    // Every setter must also emit sig_graphicsSettingsChanged, mirroring
    // GraphicsPage's per-control emit graphicsSettingsChanged() calls (see
    // graphicspage.cpp).
    const QColor newColor{Qt::red};
    QVERIFY(adapter.setProperty("backgroundColor", newColor));
    QCOMPARE(getConfig().canvas.backgroundColor.getColor().getQColor(), newColor);
    QCOMPARE(changedSpy.count(), 1);
    QCOMPARE(graphicsSettingsSpy.count(), 1);

    QVERIFY(adapter.setProperty("weatherAtmosphereIntensity", 42));
    QCOMPARE(getConfig().canvas.weatherAtmosphereIntensity.get(), 42);
    QCOMPARE(graphicsSettingsSpy.count(), 2);

    QVERIFY(adapter.setProperty("drawDoorNames", true));
    QCOMPARE(getConfig().canvas.drawDoorNames, true);
    QCOMPARE(graphicsSettingsSpy.count(), 3);
}

void TestMainWindow::advancedGraphicsModelRoundTrip()
{
    setEnteredMain();

    const auto original = getConfig().canvas.advanced.maximumFps.get();
    auto cleanup = qScopeGuard([=]() { setConfig().canvas.advanced.maximumFps.set(original); });

    AdvancedGraphicsModel model(nullptr);
    QCOMPARE(model.rowCount(), 5);

    QSignalSpy graphicsSettingsSpy(&model, &AdvancedGraphicsModel::sig_graphicsSettingsChanged);

    // Row 4 ("Maximum FPS") is a global (not 3d-only) FixedPoint<0> row; see
    // AdvancedGraphicsModel's ctor row order.
    const QModelIndex idx = model.index(4, 0);
    QCOMPARE(model.data(idx, AdvancedGraphicsModel::Is3DOnlyRole).toBool(), false);

    model.setValue(4, 100);
    QCOMPARE(getConfig().canvas.advanced.maximumFps.get(), 100);
    QCOMPARE(model.data(idx, AdvancedGraphicsModel::ValueRole).toInt(), 100);
    QCOMPARE(graphicsSettingsSpy.count(), 1);

    model.reset(4);
    QCOMPARE(getConfig().canvas.advanced.maximumFps.get(),
             getConfig().canvas.advanced.maximumFps.defaultValue);
    QCOMPARE(graphicsSettingsSpy.count(), 2);
}

void TestMainWindow::parserPageAdapterRoundTrip()
{
    setEnteredMain();

    const auto original = getConfig().parser;
    auto cleanup = qScopeGuard([=]() { setConfig().parser = original; });

    ParserPageAdapter adapter(nullptr, nullptr);
    QSignalSpy changedSpy(&adapter, &ParserPageAdapter::sig_changed);

    // A punctuation character is a valid prefix, mirroring
    // CommandPrefixValidator/isValidPrefix() (ascii::isPunct); a letter is
    // rejected and must leave the config untouched.
    QVERIFY(adapter.setPrefixChar(QStringLiteral("@")));
    QCOMPARE(getConfig().parser.prefixChar, '@');
    QCOMPARE(adapter.getPrefixChar(), QStringLiteral("@"));
    QCOMPARE(changedSpy.count(), 1);

    QVERIFY(!adapter.setPrefixChar(QStringLiteral("a")));
    QCOMPARE(getConfig().parser.prefixChar, '@');
    QCOMPARE(changedSpy.count(), 1);

    QVERIFY(!adapter.setPrefixChar(QStringLiteral("ab")));
    QCOMPARE(changedSpy.count(), 1);

    // roomNameColorFg/Bg mirror AnsiCombo::colorFromString()'s decoding of
    // the raw ANSI escape-sequence string stored in Configuration.
    setConfig().parser.roomNameColor = QStringLiteral("[1;32m");
    const auto expected = AnsiCombo::colorFromString(QStringLiteral("[1;32m"));
    QCOMPARE(adapter.getRoomNameColorFg(), expected.getFgColor());
    QCOMPARE(adapter.getRoomNameColorBg(), expected.getBgColor());
}

void TestMainWindow::generalPageAdapterRoundTrip()
{
    setEnteredMain();

    // GeneralSettings holds a ChangeMonitor and is non-copyable, so restore
    // its individual fields; the other touched groups are plain copyable
    // structs and can be snapshotted whole.
    const auto originalConnection = getConfig().connection;
    const auto originalMumeNative = getConfig().mumeNative;
    const auto originalAutoLoad = getConfig().autoLoad;
    const auto originalCharacterEncoding = getConfig().general.characterEncoding;
    const auto originalTheme = getConfig().general.getTheme();
    const bool originalCheckForUpdate = getConfig().general.checkForUpdate;
    auto cleanup = qScopeGuard([=]() {
        setConfig().connection = originalConnection;
        setConfig().mumeNative = originalMumeNative;
        setConfig().autoLoad = originalAutoLoad;
        setConfig().general.characterEncoding = originalCharacterEncoding;
        setConfig().general.setTheme(originalTheme);
        setConfig().general.checkForUpdate = originalCheckForUpdate;
    });

    GeneralPageAdapter adapter(nullptr, nullptr);
    QSignalSpy changedSpy(&adapter, &GeneralPageAdapter::sig_changed);

    QVERIFY(adapter.setProperty("remoteName", QStringLiteral("mume.org")));
    QCOMPARE(getConfig().connection.remoteServerName, QStringLiteral("mume.org"));
    QCOMPARE(changedSpy.count(), 1);

    QVERIFY(adapter.setProperty("remotePort", 4242));
    QCOMPARE(getConfig().connection.remotePort, static_cast<uint16_t>(4242));

    // themeIndex/characterEncodingIndex map 1:1 onto ThemeEnum/
    // CharacterEncodingEnum's declaration order; see the static_asserts at
    // the top of GeneralPageAdapter.cpp.
    QVERIFY(adapter.setProperty("themeIndex", 1));
    QCOMPARE(getConfig().general.getTheme(), ThemeEnum::Dark);

    QVERIFY(adapter.setProperty("characterEncodingIndex", 1));
    QCOMPARE(getConfig().general.characterEncoding, CharacterEncodingEnum::UTF8);

    QVERIFY(adapter.setProperty("emulatedExits", true));
    QCOMPARE(getConfig().mumeNative.emulatedExits, true);

    QVERIFY(adapter.setProperty("autoLoadMap", true));
    QCOMPARE(getConfig().autoLoad.autoLoadMap, true);
}

void TestMainWindow::clientPageAdapterRoundTrip()
{
    setEnteredMain();

    const auto original = getConfig().integratedClient;
    auto cleanup = qScopeGuard([=]() { setConfig().integratedClient = original; });

    ClientPageAdapter adapter(nullptr, nullptr);
    QSignalSpy changedSpy(&adapter, &ClientPageAdapter::sig_changed);

    QVERIFY(adapter.setProperty("columns", 100));
    QCOMPARE(getConfig().integratedClient.columns, 100);
    QCOMPARE(changedSpy.count(), 1);

    const QColor newColor{Qt::blue};
    QVERIFY(adapter.setProperty("foregroundColor", newColor));
    QCOMPARE(getConfig().integratedClient.foregroundColor, newColor);

    // A separator must be exactly one printable, non-space, non-backslash
    // character, mirroring CustomSeparatorValidator (see clientpage.cpp).
    QVERIFY(adapter.setCommandSeparator(QStringLiteral(";")));
    QCOMPARE(getConfig().integratedClient.commandSeparator, QStringLiteral(";"));

    QVERIFY(!adapter.setCommandSeparator(QStringLiteral("\\")));
    QCOMPARE(getConfig().integratedClient.commandSeparator, QStringLiteral(";"));

    QVERIFY(!adapter.setCommandSeparator(QStringLiteral(" ")));
    QCOMPARE(getConfig().integratedClient.commandSeparator, QStringLiteral(";"));

    QVERIFY(!adapter.setCommandSeparator(QStringLiteral("ab")));
}

QTEST_MAIN(TestMainWindow)
