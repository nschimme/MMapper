// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#include "TestPreferences.h"

#include "../src/configuration/configuration.h"
#include "../src/global/Signal2.h"
#include "../src/preferences/configdialog.h"

#include <QDialogButtonBox>
#include <QPushButton>
#include <QtTest/QtTest>

TestPreferences::TestPreferences() = default;

TestPreferences::~TestPreferences() = default;

void TestPreferences::initTestCase()
{
    setEnteredMain();
}

void TestPreferences::configValueTest()
{
    ConfigValue<int> val(10);
    QCOMPARE(val.get(), 10);
    QCOMPARE(static_cast<int>(val), 10);
    QVERIFY(val == 10);
    QVERIFY(val != 20);

    size_t notifications = 0;
    Signal2Lifetime lifetime;
    val.registerChangeCallback(lifetime, [&notifications]() { notifications++; });

    val = 20;
    QCOMPARE(val.get(), 20);
    QCOMPARE(notifications, 1u);

    // No notification if value is the same
    val = 20;
    QCOMPARE(notifications, 1u);

    // Multiple listeners
    size_t notifications2 = 0;
    val.registerChangeCallback(lifetime, [&notifications2]() { notifications2++; });
    val = 30;
    QCOMPARE(notifications, 2u);
    QCOMPARE(notifications2, 1u);

    // Copying
    ConfigValue<int> val2 = val;
    QCOMPARE(val2.get(), 30);
    // Listeners are NOT copied
    val2 = 40;
    QCOMPARE(notifications, 2u);
}

void TestPreferences::configTest()
{
    // Test basic copy/equality logic for Configuration and its settings
    Configuration c1;
    Configuration c2;
    QVERIFY(c1 == c2);

    c2.general.alwaysOnTop = !c1.general.alwaysOnTop;
    QVERIFY(c1 != c2);

    c1 = c2;
    QVERIFY(c1 == c2);

    c2.audio.setMusicVolume(1);
    QVERIFY(c1 != c2);

    c1 = c2;
    QVERIFY(c1 == c2);

    c2.canvas.drawDoorNames = !c1.canvas.drawDoorNames;
    QVERIFY(c1 != c2);

    c1 = c2;
    QVERIFY(c1 == c2);

    c2.canvas.backgroundColor = Color(255, 0, 0);
    QVERIFY(c1 != c2);

    c1 = c2;
    QVERIFY(c1 == c2);

    c2.canvas.advanced.maximumFps.set(120);
    QVERIFY(c1 != c2);

    c1 = c2;
    QVERIFY(c1 == c2);

    c2.connection.remotePort = 1234;
    QVERIFY(c1 != c2);
    c1 = c2;
    QVERIFY(c1 == c2);

    c2.parser.prefixChar = '!';
    QVERIFY(c1 != c2);
    c1 = c2;
    QVERIFY(c1 == c2);

    c2.mumeClientProtocol.internalRemoteEditor = !c1.mumeClientProtocol.internalRemoteEditor;
    QVERIFY(c1 != c2);
    c1 = c2;
    QVERIFY(c1 == c2);

    c2.mumeNative.showNotes = !c1.mumeNative.showNotes;
    QVERIFY(c1 != c2);
    c1 = c2;
    QVERIFY(c1 == c2);

    c2.account.accountName = "test";
    QVERIFY(c1 != c2);
    c1 = c2;
    QVERIFY(c1 == c2);

    c2.autoLoad.autoLoadMap = !c1.autoLoad.autoLoadMap;
    QVERIFY(c1 != c2);
    c1 = c2;
    QVERIFY(c1 == c2);

    c2.autoLog.autoLog = !c1.autoLog.autoLog;
    QVERIFY(c1 != c2);
    c1 = c2;
    QVERIFY(c1 == c2);

    c2.pathMachine.maxPaths = 42;
    QVERIFY(c1 != c2);
    c1 = c2;
    QVERIFY(c1 == c2);

    c2.groupManager.npcHide = !c1.groupManager.npcHide;
    QVERIFY(c1 != c2);
    c1 = c2;
    QVERIFY(c1 == c2);

    c2.mumeClock.startEpoch = 123456789;
    QVERIFY(c1 != c2);
    c1 = c2;
    QVERIFY(c1 == c2);

    c2.adventurePanel.setDisplayXPStatus(!c1.adventurePanel.getDisplayXPStatus());
    QVERIFY(c1 != c2);
    c1 = c2;
    QVERIFY(c1 == c2);

    c2.integratedClient.audibleBell = !c1.integratedClient.audibleBell;
    QVERIFY(c1 != c2);
    c1 = c2;
    QVERIFY(c1 == c2);

    c2.roomPanel.geometry = QByteArray("test");
    QVERIFY(c1 != c2);
    c1 = c2;
    QVERIFY(c1 == c2);

    c2.infomarksDialog.geometry = QByteArray("test");
    QVERIFY(c1 != c2);
    c1 = c2;
    QVERIFY(c1 == c2);

    c2.roomEditDialog.geometry = QByteArray("test");
    QVERIFY(c1 != c2);
    c1 = c2;
    QVERIFY(c1 == c2);

    c2.findRoomsDialog.geometry = QByteArray("test");
    QVERIFY(c1 != c2);
    c1 = c2;
    QVERIFY(c1 == c2);

    c2.colorSettings.BACKGROUND = Color(0, 255, 0);
    QVERIFY(c1 != c2);
    c1 = c2;
    QVERIFY(c1 == c2);
}

void TestPreferences::configDialogTest()
{
    const bool originalAlwaysOnTop = getConfig().general.alwaysOnTop;

    ConfigDialog dialog;
    dialog.show();

    // Verify Apply button is disabled initially
    auto *applyButton = dialog.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Apply);
    QVERIFY(applyButton != nullptr);
    QVERIFY(!applyButton->isEnabled());

    // Change a setting in the working config
    dialog.m_workingConfig.general.alwaysOnTop = !originalAlwaysOnTop;
    dialog.slot_updateApplyButton();

    QVERIFY(applyButton->isEnabled());

    // Verify global config is NOT updated yet
    QCOMPARE(getConfig().general.alwaysOnTop.get(), originalAlwaysOnTop);

    // Click Apply
    dialog.slot_apply();

    // Verify Apply button is disabled again
    QVERIFY(!applyButton->isEnabled());

    // Verify global config IS updated now
    QCOMPARE(getConfig().general.alwaysOnTop.get(), !originalAlwaysOnTop);

    // Cleanup
    setConfig().general.alwaysOnTop = originalAlwaysOnTop;
}

QTEST_MAIN(TestPreferences)
