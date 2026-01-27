// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "TestHotkeyManager.h"

#include "../src/client/Hotkey.h"
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

#define CHECK_HK(hk, cmd) QCOMPARE(manager.getCommand(hk).value_or(""), std::string(cmd))

    // Test all base keys defined in macro
#define X_TEST_KEY(id, name, qkey, num) \
    QVERIFY(manager.setHotkey(Hotkey{name}, "cmd_" name)); \
    CHECK_HK(Hotkey{name}, "cmd_" name); \
    CHECK_HK((Hotkey{qkey, Qt::NoModifier, num}), "cmd_" name);

    XFOREACH_HOTKEY_BASE_KEYS(X_TEST_KEY)
#undef X_TEST_KEY

    // Test that modifiers are normalized to canonical order: SHIFT+CTRL+ALT+META

    // Set a hotkey with non-canonical modifier order
    QVERIFY(manager.setHotkey(Hotkey{"ALT+CTRL+F1"}, "test1"));

    // Should be retrievable with canonical order
    CHECK_HK(Hotkey{"CTRL+ALT+F1"}, "test1");

    // Should also be retrievable with the original order (due to normalization)
    CHECK_HK(Hotkey{"ALT+CTRL+F1"}, "test1");

    // Test all modifier combinations normalize correctly
    QVERIFY(manager.setHotkey(Hotkey{"META+ALT+SHIFT+CTRL+F2"}, "test2"));
    CHECK_HK(Hotkey{"SHIFT+CTRL+ALT+META+F2"}, "test2");

    // Test that case is normalized to uppercase
    QVERIFY(manager.setHotkey(Hotkey{"ctrl+f3"}, "test3"));
    CHECK_HK(Hotkey{"CTRL+F3"}, "test3");

    // Test CONTROL alias normalizes to CTRL
    QVERIFY(manager.setHotkey(Hotkey{"CONTROL+F4"}, "test4"));
    CHECK_HK(Hotkey{"CTRL+F4"}, "test4");

    // Test CMD/COMMAND aliases normalize to META
    QVERIFY(manager.setHotkey(Hotkey{"CMD+F5"}, "test5"));
    CHECK_HK(Hotkey{"META+F5"}, "test5");

    QVERIFY(manager.setHotkey(Hotkey{"COMMAND+F6"}, "test6"));
    CHECK_HK(Hotkey{"META+F6"}, "test6");
#undef CHECK_HK
}

void TestHotkeyManager::importExportRoundTripTest()
{
    HotkeyManager manager;
    manager.clear();
#define CHECK_HK(hk, cmd) QCOMPARE(manager.getCommand(hk).value_or(""), std::string(cmd))

    // Verify all hotkeys can be set and retrieved
    QVERIFY(manager.setHotkey(Hotkey{"F1"}, "look"));
    QVERIFY(manager.setHotkey(Hotkey{"CTRL+F2"}, "open exit n"));
    QVERIFY(manager.setHotkey(Hotkey{"SHIFT+ALT+F3"}, "pick exit s"));
    QVERIFY(manager.setHotkey(Hotkey{"NUMPAD8"}, "n"));

    CHECK_HK(Hotkey{"F1"}, "look");
    CHECK_HK(Hotkey{"CTRL+F2"}, "open exit n");
    CHECK_HK(Hotkey{"SHIFT+ALT+F3"}, "pick exit s");
    CHECK_HK(Hotkey{"NUMPAD8"}, "n");

    // Verify total count
    QCOMPARE(static_cast<int>(manager.getAllHotkeys().size()), 4);
#undef CHECK_HK
}

void TestHotkeyManager::importEdgeCasesTest()
{
    HotkeyManager manager;
    manager.clear();

    // Test command with multiple spaces (should preserve spaces in command)
    std::ignore = manager.setHotkey(Hotkey{"F1"}, "cast 'cure light'");
    QCOMPARE(manager.getCommand(Hotkey{"F1"}).value_or(""), std::string("cast 'cure light'"));

    // Test leading/trailing whitespace handling in key name
    QVERIFY(manager.setHotkey(Hotkey{"  F4  "}, "command with spaces"));
    QCOMPARE(manager.getCommand(Hotkey{"F4"}).value_or(""), std::string("command with spaces"));
}

void TestHotkeyManager::resetToDefaultsTest()
{
    HotkeyManager manager;
    manager.clear();
#define CHECK_HK(hk, cmd) QCOMPARE(manager.getCommand(hk).value_or(""), std::string(cmd))

    // Set custom hotkeys
    std::ignore = manager.setHotkey(Hotkey{"F1"}, "custom");
    std::ignore = manager.setHotkey(Hotkey{"F2"}, "another");
    CHECK_HK(Hotkey{"F1"}, "custom");
    QCOMPARE(static_cast<int>(manager.getAllHotkeys().size()), 2);

    // Reset to defaults
    manager.resetToDefaults();

    // Verify defaults are restored
    CHECK_HK(Hotkey{"NUMPAD8"}, "north");
    CHECK_HK(Hotkey{"NUMPAD4"}, "west");
    CHECK_HK(Hotkey{"CTRL+NUMPAD8"}, "open exit north");
    CHECK_HK(Hotkey{"ALT+NUMPAD8"}, "close exit north");
    CHECK_HK(Hotkey{"SHIFT+NUMPAD8"}, "pick exit north");

    // F1 is in defaults
    CHECK_HK(Hotkey{"F1"}, "F1");

    // Verify defaults are non-empty (don't assert exact count to avoid brittleness)
    QVERIFY(!manager.getAllHotkeys().empty());
#undef CHECK_HK
}

void TestHotkeyManager::exportSortOrderTest()
{
    HotkeyManager manager;
    manager.clear();

    std::ignore = manager.setHotkey(Hotkey{"F1"}, "cmd1");
    std::ignore = manager.setHotkey(Hotkey{"F2"}, "cmd2");

    auto hotkeys = manager.getAllHotkeys();
    QCOMPARE(static_cast<int>(hotkeys.size()), 2);
}

void TestHotkeyManager::setHotkeyTest()
{
    HotkeyManager manager;
    manager.clear();
#define CHECK_HK(hk, cmd) QCOMPARE(manager.getCommand(hk).value_or(""), std::string(cmd))

    // Test setting a new hotkey directly
    QVERIFY(manager.setHotkey(Hotkey{"F1"}, "look"));
    CHECK_HK(Hotkey{"F1"}, "look");
    QCOMPARE(static_cast<int>(manager.getAllHotkeys().size()), 1);

    // Test setting another hotkey
    QVERIFY(manager.setHotkey(Hotkey{"F2"}, "flee"));
    CHECK_HK(Hotkey{"F2"}, "flee");
    QCOMPARE(static_cast<int>(manager.getAllHotkeys().size()), 2);

    // Test updating an existing hotkey (should replace, not add)
    QVERIFY(manager.setHotkey(Hotkey{"F1"}, "inventory"));
    CHECK_HK(Hotkey{"F1"}, "inventory");
    QCOMPARE(static_cast<int>(manager.getAllHotkeys().size()), 2); // Still 2, not 3

    // Test setting hotkey with modifiers
    QVERIFY(manager.setHotkey(Hotkey{"CTRL+F3"}, "open exit n"));
    CHECK_HK(Hotkey{"CTRL+F3"}, "open exit n");
    QCOMPARE(static_cast<int>(manager.getAllHotkeys().size()), 3);
#undef CHECK_HK
}

void TestHotkeyManager::removeHotkeyTest()
{
    HotkeyManager manager;
    manager.clear();
#define CHECK_HK(hk, cmd) QCOMPARE(manager.getCommand(hk).value_or(""), std::string(cmd))

    std::ignore = manager.setHotkey(Hotkey{"F1"}, "look");
    std::ignore = manager.setHotkey(Hotkey{"F2"}, "flee");
    std::ignore = manager.setHotkey(Hotkey{"CTRL+F3"}, "open exit n");
    QCOMPARE(static_cast<int>(manager.getAllHotkeys().size()), 3);

    // Test removing a hotkey
    manager.removeHotkey(Hotkey{"F1"});
    CHECK_HK(Hotkey{"F1"}, ""); // Should be empty now
    QCOMPARE(static_cast<int>(manager.getAllHotkeys().size()), 2);

    // Test removing hotkey with modifiers
    manager.removeHotkey(Hotkey{"CTRL+F3"});
    CHECK_HK(Hotkey{"CTRL+F3"}, "");
    QCOMPARE(static_cast<int>(manager.getAllHotkeys().size()), 1);
#undef CHECK_HK
}

void TestHotkeyManager::hasHotkeyTest()
{
    HotkeyManager manager;
    manager.clear();

    std::ignore = manager.setHotkey(Hotkey{"F1"}, "look");
    std::ignore = manager.setHotkey(Hotkey{"CTRL+F2"}, "flee");

    // Test hasHotkey returns true for existing keys
    QVERIFY(manager.hasHotkey(Hotkey{"F1"}));
    QVERIFY(manager.hasHotkey(Hotkey{"CTRL+F2"}));

    // Test hasHotkey returns false for non-existent keys
    QVERIFY(!manager.hasHotkey(Hotkey{"F3"}));
}

void TestHotkeyManager::invalidKeyValidationTest()
{
    HotkeyManager manager;
    manager.clear();

    // Test that invalid base keys are rejected
    QVERIFY(!manager.setHotkey(Hotkey{"F13"}, "invalid"));
    QCOMPARE(manager.getCommand(Hotkey{"F13"}).value_or(""), std::string("")); // Should not be set
    QCOMPARE(static_cast<int>(manager.getAllHotkeys().size()), 0);

    // Test typo in key name
    QVERIFY(!manager.setHotkey(Hotkey{"NUMPDA8"}, "typo")); // Typo: NUMPDA instead of NUMPAD
}

void TestHotkeyManager::duplicateKeyBehaviorTest()
{
    HotkeyManager manager;
    manager.clear();

    // Test that setHotkey replaces existing entry
    std::ignore = manager.setHotkey(Hotkey{"F1"}, "original");
    QCOMPARE(manager.getCommand(Hotkey{"F1"}).value_or(""), std::string("original"));
    QCOMPARE(static_cast<int>(manager.getAllHotkeys().size()), 1);

    QVERIFY(manager.setHotkey(Hotkey{"F1"}, "replaced"));
    QCOMPARE(manager.getCommand(Hotkey{"F1"}).value_or(""), std::string("replaced"));
    QCOMPARE(static_cast<int>(manager.getAllHotkeys().size()), 1); // Still 1, not 2
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
#define CHECK_HK(hk, cmd) QCOMPARE(manager.getCommand(hk).value_or(""), std::string(cmd))

    // Set hotkeys for testing
    std::ignore = manager.setHotkey(Hotkey{"F1"}, "look");
    std::ignore = manager.setHotkey(Hotkey{"CTRL+F2"}, "flee");
    std::ignore = manager.setHotkey(Hotkey{"NUMPAD8"}, "n");
    std::ignore = manager.setHotkey(Hotkey{"CTRL+NUMPAD5"}, "s");
    std::ignore = manager.setHotkey(Hotkey{"SHIFT+ALT+UP"}, "north");

    // Test direct lookup for function keys (isNumpad=false)
    CHECK_HK((Hotkey{Qt::Key_F1, Qt::NoModifier, false}), "look");
    CHECK_HK((Hotkey{Qt::Key_F2, Qt::ControlModifier, false}), "flee");

    // Test that wrong modifiers don't match
    CHECK_HK((Hotkey{Qt::Key_F1, Qt::ControlModifier, false}), "");

    // Test numpad keys (isNumpad=true) - Qt::Key_8 with isNumpad=true
    CHECK_HK((Hotkey{Qt::Key_8, Qt::NoModifier, true}), "n");
    CHECK_HK((Hotkey{Qt::Key_5, Qt::ControlModifier, true}), "s");

    // Test that numpad keys don't match non-numpad lookups
    CHECK_HK((Hotkey{Qt::Key_8, Qt::NoModifier, false}), "");

    // Test arrow keys (isNumpad=false)
    CHECK_HK((Hotkey{Qt::Key_Up, (Qt::ShiftModifier | Qt::AltModifier), false}), "north");
#undef CHECK_HK
}

QTEST_MAIN(TestHotkeyManager)
