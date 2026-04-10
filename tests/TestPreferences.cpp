// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#include "TestPreferences.h"

#include "../src/configuration/configuration.h"
#include "../src/global/Signal2.h"

#include <QSettings>
#include <QTemporaryFile>
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

void TestPreferences::configPersistenceTest()
{
    QTemporaryFile tempFile;
    QVERIFY(tempFile.open());
    QString fileName = tempFile.fileName();
    tempFile.close();

    {
        QSettings settings(fileName, QSettings::IniFormat);
        ConfigValue<int> val("Section/Key", "Label", 50);
        val = 75;
        val.write(settings);
        settings.sync();
    }

    {
        QSettings settings(fileName, QSettings::IniFormat);
        ConfigValue<int> val("Section/Key", "Label", 50);
        val.read(settings);
        QCOMPARE(val.get(), 75);
    }
}

void TestPreferences::configLegacyColorTest()
{
    QTemporaryFile tempFile;
    QVERIFY(tempFile.open());
    QString fileName = tempFile.fileName();
    tempFile.close();

    // Test QString hex legacy
    {
        QSettings settings(fileName, QSettings::IniFormat);
        settings.setValue("Colors/background", "#FF0000");
        settings.sync();
    }

    {
        QSettings settings(fileName, QSettings::IniFormat);
        ConfigValue<Color> val("Colors/background", "Background", Color(0, 0, 0));
        val.read(settings);
        QCOMPARE(val.get(), Color(255, 0, 0));
    }

    // Test QColor legacy
    {
        QSettings settings(fileName, QSettings::IniFormat);
        settings.setValue("Colors/background", QColor(Qt::green));
        settings.sync();
    }

    {
        QSettings settings(fileName, QSettings::IniFormat);
        ConfigValue<Color> val("Colors/background", "Background", Color(0, 0, 0));
        val.read(settings);
        QCOMPARE(val.get(), Color(0, 255, 0));
    }
}

void TestPreferences::configValidatorTest()
{
    ConfigValue<int> val("Key", "Label", 50, [](int v) { return std::clamp(v, 0, 100); });

    QTemporaryFile tempFile;
    QVERIFY(tempFile.open());
    QString fileName = tempFile.fileName();
    tempFile.close();

    {
        QSettings settings(fileName, QSettings::IniFormat);
        settings.setValue("Key", 150);
        settings.sync();
    }

    {
        QSettings settings(fileName, QSettings::IniFormat);
        val.read(settings);
        QCOMPARE(val.get(), 100);
    }

    {
        QSettings settings(fileName, QSettings::IniFormat);
        settings.setValue("Key", -50);
        settings.sync();
    }

    {
        QSettings settings(fileName, QSettings::IniFormat);
        val.read(settings);
        QCOMPARE(val.get(), 0);
    }
}

QTEST_MAIN(TestPreferences)
