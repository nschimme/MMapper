// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "TestHotkeyManager.h"

#include "../src/client/HotkeyManager.h"
#include "../src/configuration/configuration.h"

#include <QCoreApplication>
#include <QSettings>
#include <QStringList>
#include <QtTest/QtTest>

TestHotkeyManager::TestHotkeyManager() = default;
TestHotkeyManager::~TestHotkeyManager() = default;

void TestHotkeyManager::initTestCase()
{
    // Save original QSettings namespace to avoid polluting real user settings
    m_originalOrganization = QCoreApplication::organizationName();
    m_originalApplication = QCoreApplication::applicationName();

    // Use test-specific namespace for isolation
    QCoreApplication::setOrganizationName(QStringLiteral("MMapperTest"));
    QCoreApplication::setApplicationName(QStringLiteral("HotkeyManagerTest"));

    setEnteredMain();
}

void TestHotkeyManager::cleanupTestCase()
{
    // Restore original QSettings namespace
    QCoreApplication::setOrganizationName(m_originalOrganization);
    QCoreApplication::setApplicationName(m_originalApplication);
}

void TestHotkeyManager::keyNormalizationTest()
{
    HotkeyManager manager;
    manager.clear();

    // Test all base keys defined in macro
#define X_TEST_KEY(id, name, qkey, num) \
    QVERIFY(manager.setHotkey(name, "cmd_" name)); \
    QCOMPARE(manager.getCommandQString(name).value_or(QString()), QString("cmd_" name)); \
    QCOMPARE(manager.getCommandQString(qkey, Qt::NoModifier, num).value_or(QString()), \
             QString("cmd_" name));

    XFOREACH_HOTKEY_BASE_KEYS(X_TEST_KEY)
#undef X_TEST_KEY

    // Test that modifiers are normalized to canonical order: CTRL+SHIFT+ALT+META
    // Set a hotkey with non-canonical modifier order
    QVERIFY(manager.setHotkey("ALT+CTRL+F1", "test1"));

    // Should be retrievable with canonical order
    QCOMPARE(manager.getCommandQString("CTRL+ALT+F1").value_or(QString()), QString("test1"));

    // Should also be retrievable with the original order (due to normalization)
    QCOMPARE(manager.getCommandQString("ALT+CTRL+F1").value_or(QString()), QString("test1"));

    // Test all modifier combinations normalize correctly
    QVERIFY(manager.setHotkey("META+ALT+SHIFT+CTRL+F2", "test2"));
    QCOMPARE(manager.getCommandQString("CTRL+SHIFT+ALT+META+F2").value_or(QString()),
             QString("test2"));

    // Test that case is normalized to uppercase
    QVERIFY(manager.setHotkey("ctrl+f3", "test3"));
    QCOMPARE(manager.getCommandQString("CTRL+F3").value_or(QString()), QString("test3"));

    // Test CONTROL alias normalizes to CTRL
    QVERIFY(manager.setHotkey("CONTROL+F4", "test4"));
    QCOMPARE(manager.getCommandQString("CTRL+F4").value_or(QString()), QString("test4"));

    // Test CMD/COMMAND aliases normalize to META
    QVERIFY(manager.setHotkey("CMD+F5", "test5"));
    QCOMPARE(manager.getCommandQString("META+F5").value_or(QString()), QString("test5"));

    QVERIFY(manager.setHotkey("COMMAND+F6", "test6"));
    QCOMPARE(manager.getCommandQString("META+F6").value_or(QString()), QString("test6"));
}

void TestHotkeyManager::importExportRoundTripTest()
{
    HotkeyManager manager;
    manager.clear();

    // Verify all hotkeys can be set and retrieved
    QVERIFY(manager.setHotkey("F1", "look"));
    QVERIFY(manager.setHotkey("CTRL+F2", "open exit n"));
    QVERIFY(manager.setHotkey("SHIFT+ALT+F3", "pick exit s"));
    QVERIFY(manager.setHotkey("NUMPAD8", "n"));

    QCOMPARE(manager.getCommandQString("F1").value_or(QString()), QString("look"));
    QCOMPARE(manager.getCommandQString("CTRL+F2").value_or(QString()), QString("open exit n"));
    QCOMPARE(manager.getCommandQString("SHIFT+ALT+F3").value_or(QString()), QString("pick exit s"));
    QCOMPARE(manager.getCommandQString("NUMPAD8").value_or(QString()), QString("n"));

    // Verify total count
    QCOMPARE(manager.getAllHotkeys().size(), 4);
}

void TestHotkeyManager::importEdgeCasesTest()
{
    HotkeyManager manager;
    manager.clear();

    // Test command with multiple spaces (should preserve spaces in command)
    std::ignore = manager.setHotkey("F1", "cast 'cure light'");
    QCOMPARE(manager.getCommandQString("F1").value_or(QString()), QString("cast 'cure light'"));

    // Test leading/trailing whitespace handling in key name
    QVERIFY(manager.setHotkey("  F4  ", "command with spaces"));
    QCOMPARE(manager.getCommandQString("F4").value_or(QString()), QString("command with spaces"));
}

void TestHotkeyManager::resetToDefaultsTest()
{
    HotkeyManager manager;
    manager.clear();

    // Set custom hotkeys
    std::ignore = manager.setHotkey("F1", "custom");
    std::ignore = manager.setHotkey("F2", "another");
    QCOMPARE(manager.getCommandQString("F1").value_or(QString()), QString("custom"));
    QCOMPARE(manager.getAllHotkeys().size(), 2);

    // Reset to defaults
    manager.resetToDefaults();

    // Verify defaults are restored
    QCOMPARE(manager.getCommandQString("NUMPAD8").value_or(QString()), QString("n"));
    QCOMPARE(manager.getCommandQString("NUMPAD4").value_or(QString()), QString("w"));
    QCOMPARE(manager.getCommandQString("CTRL+NUMPAD8").value_or(QString()), QString("open exit n"));
    QCOMPARE(manager.getCommandQString("ALT+NUMPAD8").value_or(QString()), QString("close exit n"));
    QCOMPARE(manager.getCommandQString("SHIFT+NUMPAD8").value_or(QString()), QString("pick exit n"));

    // F1 is not in defaults, should be empty
    QCOMPARE(manager.getCommandQString("F1").value_or(QString()), QString());

    // Verify defaults are non-empty (don't assert exact count to avoid brittleness)
    QVERIFY(!manager.getAllHotkeys().empty());
}

void TestHotkeyManager::exportSortOrderTest()
{
    // This test is less relevant now as we don't have import/export CLI format directly in manager
    // But we can check that getAllHotkeys returns everything we set
    HotkeyManager manager;
    manager.clear();

    std::ignore = manager.setHotkey("F1", "cmd1");
    std::ignore = manager.setHotkey("F2", "cmd2");

    auto hotkeys = manager.getAllHotkeys();
    QCOMPARE(hotkeys.size(), 2);
}

void TestHotkeyManager::setHotkeyTest()
{
    HotkeyManager manager;
    manager.clear();

    // Test setting a new hotkey directly
    QVERIFY(manager.setHotkey("F1", "look"));
    QCOMPARE(manager.getCommandQString("F1").value_or(QString()), QString("look"));
    QCOMPARE(manager.getAllHotkeys().size(), 1);

    // Test setting another hotkey
    QVERIFY(manager.setHotkey("F2", "flee"));
    QCOMPARE(manager.getCommandQString("F2").value_or(QString()), QString("flee"));
    QCOMPARE(manager.getAllHotkeys().size(), 2);

    // Test updating an existing hotkey (should replace, not add)
    QVERIFY(manager.setHotkey("F1", "inventory"));
    QCOMPARE(manager.getCommandQString("F1").value_or(QString()), QString("inventory"));
    QCOMPARE(manager.getAllHotkeys().size(), 2); // Still 2, not 3

    // Test setting hotkey with modifiers
    QVERIFY(manager.setHotkey("CTRL+F3", "open exit n"));
    QCOMPARE(manager.getCommandQString("CTRL+F3").value_or(QString()), QString("open exit n"));
    QCOMPARE(manager.getAllHotkeys().size(), 3);
}

void TestHotkeyManager::removeHotkeyTest()
{
    HotkeyManager manager;
    manager.clear();

    std::ignore = manager.setHotkey("F1", "look");
    std::ignore = manager.setHotkey("F2", "flee");
    std::ignore = manager.setHotkey("CTRL+F3", "open exit n");
    QCOMPARE(manager.getAllHotkeys().size(), 3);

    // Test removing a hotkey
    manager.removeHotkey("F1");
    QCOMPARE(manager.getCommandQString("F1").value_or(QString()),
             QString()); // Should be empty now
    QCOMPARE(manager.getAllHotkeys().size(), 2);

    // Test removing hotkey with modifiers
    manager.removeHotkey("CTRL+F3");
    QCOMPARE(manager.getCommandQString("CTRL+F3").value_or(QString()), QString());
    QCOMPARE(manager.getAllHotkeys().size(), 1);
}

void TestHotkeyManager::hasHotkeyTest()
{
    HotkeyManager manager;
    manager.clear();

    std::ignore = manager.setHotkey("F1", "look");
    std::ignore = manager.setHotkey("CTRL+F2", "flee");

    // Test hasHotkey returns true for existing keys
    QVERIFY(manager.hasHotkey("F1"));
    QVERIFY(manager.hasHotkey("CTRL+F2"));

    // Test hasHotkey returns false for non-existent keys
    QVERIFY(!manager.hasHotkey("F3"));
}

void TestHotkeyManager::invalidKeyValidationTest()
{
    HotkeyManager manager;
    manager.clear();

    // Test that invalid base keys are rejected
    QVERIFY(!manager.setHotkey("F13", "invalid"));
    QCOMPARE(manager.getCommandQString("F13").value_or(QString()), QString()); // Should not be set
    QCOMPARE(manager.getAllHotkeys().size(), 0);

    // Test typo in key name
    QVERIFY(!manager.setHotkey("NUMPDA8", "typo")); // Typo: NUMPDA instead of NUMPAD
}

void TestHotkeyManager::duplicateKeyBehaviorTest()
{
    HotkeyManager manager;
    manager.clear();

    // Test that setHotkey replaces existing entry
    std::ignore = manager.setHotkey("F1", "original");
    QCOMPARE(manager.getCommandQString("F1").value_or(QString()), QString("original"));
    QCOMPARE(manager.getAllHotkeys().size(), 1);

    QVERIFY(manager.setHotkey("F1", "replaced"));
    QCOMPARE(manager.getCommandQString("F1").value_or(QString()), QString("replaced"));
    QCOMPARE(manager.getAllHotkeys().size(), 1); // Still 1, not 2
}

void TestHotkeyManager::commentPreservationTest()
{
    // This test is no longer relevant as we moved to a structured QSettings format
}

void TestHotkeyManager::settingsPersistenceTest()
{
    HotkeyManager manager;

    // Manager should have loaded something (either defaults or saved settings)
    // Just verify it's in a valid state
    QVERIFY(!manager.getAllHotkeys().empty());
}

void TestHotkeyManager::directLookupTest()
{
    HotkeyManager manager;
    manager.clear();

    // Set hotkeys for testing
    std::ignore = manager.setHotkey("F1", "look");
    std::ignore = manager.setHotkey("CTRL+F2", "flee");
    std::ignore = manager.setHotkey("NUMPAD8", "n");
    std::ignore = manager.setHotkey("CTRL+NUMPAD5", "s");
    std::ignore = manager.setHotkey("SHIFT+ALT+UP", "north");

    // Test direct lookup for function keys (isNumpad=false)
    QCOMPARE(manager.getCommandQString(Qt::Key_F1, Qt::NoModifier, false).value_or(QString()),
             QString("look"));
    QCOMPARE(manager.getCommandQString(Qt::Key_F2, Qt::ControlModifier, false).value_or(QString()),
             QString("flee"));

    // Test that wrong modifiers don't match
    QCOMPARE(manager.getCommandQString(Qt::Key_F1, Qt::ControlModifier, false).value_or(QString()),
             QString());

    // Test numpad keys (isNumpad=true) - Qt::Key_8 with isNumpad=true
    QCOMPARE(manager.getCommandQString(Qt::Key_8, Qt::NoModifier, true).value_or(QString()),
             QString("n"));
    QCOMPARE(manager.getCommandQString(Qt::Key_5, Qt::ControlModifier, true).value_or(QString()),
             QString("s"));

    // Test that numpad keys don't match non-numpad lookups
    QCOMPARE(manager.getCommandQString(Qt::Key_8, Qt::NoModifier, false).value_or(QString()),
             QString());

    // Test arrow keys (isNumpad=false)
    QCOMPARE(manager.getCommandQString(Qt::Key_Up, Qt::ShiftModifier | Qt::AltModifier, false)
                 .value_or(QString()),
             QString("north"));
}

QTEST_MAIN(TestHotkeyManager)
