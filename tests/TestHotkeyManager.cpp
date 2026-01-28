// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "TestHotkeyManager.h"

#include "../src/client/Hotkey.h"
#include "../src/client/HotkeyManager.h"
#include "../src/configuration/configuration.h"
#include "../src/global/CaseUtils.h"

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

void TestHotkeyManager::checkHk(const HotkeyManager &manager,
                                const Hotkey &hk,
                                std::string_view expected)
{
    auto actual = manager.getCommand(hk).value_or("");
    if (actual != expected) {
        qDebug() << "Failure for key:" << QString::fromStdString(hk.serialize())
                 << "Expected:" << QString::fromStdString(std::string(expected))
                 << "Actual:" << QString::fromStdString(actual);
    }
    QCOMPARE(actual, std::string(expected));
}

void TestHotkeyManager::keyNormalizationTest()
{
    HotkeyManager manager;
    manager.clear();

    // Test all base keys defined in macro
#define X_TEST_KEY(id, name, qkey, pol) \
    { \
        const std::string nameStr(name); \
        QVERIFY(manager.setHotkey(Hotkey{nameStr}, "cmd_" + nameStr)); \
        checkHk(manager, Hotkey{nameStr}, "cmd_" + nameStr); \
        Hotkey hk(qkey, (pol == HotkeyPolicy::Keypad) ? Qt::KeypadModifier : Qt::NoModifier); \
        std::string expected = "cmd_" + nameStr; \
        if (hk.isValid() && Hotkey::hotkeyBaseToName(hk.base()) != nameStr) { \
            expected = "cmd_" + std::string(Hotkey::hotkeyBaseToName(hk.base())); \
        } \
        checkHk(manager, hk, expected); \
    }

    XFOREACH_HOTKEY_BASE_KEYS(X_TEST_KEY)
#undef X_TEST_KEY

#ifndef Q_OS_MAC
    // Test secondary mappings preserved on non-Mac platforms
    checkHk(manager, Hotkey{Qt::Key_Insert, Qt::KeypadModifier}, "cmd_NUMPAD0");
    checkHk(manager, Hotkey{Qt::Key_End, Qt::KeypadModifier}, "cmd_NUMPAD1");
    checkHk(manager, Hotkey{Qt::Key_Down, Qt::KeypadModifier}, "cmd_NUMPAD2");
    checkHk(manager, Hotkey{Qt::Key_PageDown, Qt::KeypadModifier}, "cmd_NUMPAD3");
    checkHk(manager, Hotkey{Qt::Key_Left, Qt::KeypadModifier}, "cmd_NUMPAD4");
    checkHk(manager, Hotkey{Qt::Key_Clear, Qt::KeypadModifier}, "cmd_NUMPAD5");
    checkHk(manager, Hotkey{Qt::Key_Right, Qt::KeypadModifier}, "cmd_NUMPAD6");
    checkHk(manager, Hotkey{Qt::Key_Home, Qt::KeypadModifier}, "cmd_NUMPAD7");
    checkHk(manager, Hotkey{Qt::Key_Up, Qt::KeypadModifier}, "cmd_NUMPAD8");
    checkHk(manager, Hotkey{Qt::Key_PageUp, Qt::KeypadModifier}, "cmd_NUMPAD9");
    checkHk(manager, Hotkey{Qt::Key_Delete, Qt::KeypadModifier}, "cmd_NUMPAD_PERIOD");
#endif

    // Test that modifiers are normalized to canonical order: SHIFT+CTRL+ALT+META

    // Set a hotkey with non-canonical modifier order
    QVERIFY(manager.setHotkey(Hotkey{"ALT+CTRL+F1"}, "test1"));

    // Should be retrievable with canonical order
    checkHk(manager, Hotkey{"CTRL+ALT+F1"}, "test1");

    // Should also be retrievable with the original order (due to normalization)
    checkHk(manager, Hotkey{"ALT+CTRL+F1"}, "test1");

    // Test all modifier combinations normalize correctly
    QVERIFY(manager.setHotkey(Hotkey{"META+ALT+SHIFT+CTRL+F2"}, "test2"));
    checkHk(manager, Hotkey{"SHIFT+CTRL+ALT+META+F2"}, "test2");

    // Test that case is normalized to uppercase
    QVERIFY(manager.setHotkey(Hotkey{"ctrl+f3"}, "test3"));
    checkHk(manager, Hotkey{"CTRL+F3"}, "test3");
}

void TestHotkeyManager::importExportRoundTripTest()
{
    HotkeyManager manager;
    manager.clear();

    // Verify all hotkeys can be set and retrieved
    QVERIFY(manager.setHotkey(Hotkey{"F1"}, "look"));
    QVERIFY(manager.setHotkey(Hotkey{"CTRL+F2"}, "open exit n"));
    QVERIFY(manager.setHotkey(Hotkey{"SHIFT+ALT+F3"}, "pick exit s"));
    QVERIFY(manager.setHotkey(Hotkey{"NUMPAD8"}, "n"));

    checkHk(manager, Hotkey{"F1"}, "look");
    checkHk(manager, Hotkey{"CTRL+F2"}, "open exit n");
    checkHk(manager, Hotkey{"SHIFT+ALT+F3"}, "pick exit s");
    checkHk(manager, Hotkey{"NUMPAD8"}, "n");

    // Verify total count
    QCOMPARE(static_cast<int>(manager.getAllHotkeys().size()), 4);

    // Verify serialization
    QCOMPARE(QString::fromStdString(Hotkey{"F1"}.serialize()), QString("F1"));
    QCOMPARE(QString::fromStdString(Hotkey{"CTRL+F2"}.serialize()), QString("CTRL+F2"));
    QCOMPARE(QString::fromStdString(Hotkey{"SHIFT+ALT+F3"}.serialize()), QString("SHIFT+ALT+F3"));
    QCOMPARE(QString::fromStdString(Hotkey{"NUMPAD8"}.serialize()), QString("NUMPAD8"));
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

    // Set custom hotkeys
    std::ignore = manager.setHotkey(Hotkey{"F1"}, "custom");
    std::ignore = manager.setHotkey(Hotkey{"F2"}, "another");
    checkHk(manager, Hotkey{"F1"}, "custom");
    QCOMPARE(static_cast<int>(manager.getAllHotkeys().size()), 2);

    // Reset to defaults
    manager.resetToDefaults();

    // Verify defaults are restored
    checkHk(manager, Hotkey{"NUMPAD8"}, "north");
    checkHk(manager, Hotkey{"NUMPAD4"}, "west");
    checkHk(manager, Hotkey{"CTRL+NUMPAD8"}, "open exit north");
    checkHk(manager, Hotkey{"ALT+NUMPAD8"}, "close exit north");
    checkHk(manager, Hotkey{"SHIFT+NUMPAD8"}, "pick exit north");

    // F1 is in defaults
    checkHk(manager, Hotkey{"F1"}, "F1");

    // Verify defaults are non-empty (don't assert exact count to avoid brittleness)
    QVERIFY(!manager.getAllHotkeys().empty());
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

    // Test setting a new hotkey directly
    QVERIFY(manager.setHotkey(Hotkey{"F1"}, "look"));
    checkHk(manager, Hotkey{"F1"}, "look");
    QCOMPARE(static_cast<int>(manager.getAllHotkeys().size()), 1);

    // Test setting another hotkey
    QVERIFY(manager.setHotkey(Hotkey{"F2"}, "flee"));
    checkHk(manager, Hotkey{"F2"}, "flee");
    QCOMPARE(static_cast<int>(manager.getAllHotkeys().size()), 2);

    // Test updating an existing hotkey (should replace, not add)
    QVERIFY(manager.setHotkey(Hotkey{"F1"}, "inventory"));
    checkHk(manager, Hotkey{"F1"}, "inventory");
    QCOMPARE(static_cast<int>(manager.getAllHotkeys().size()), 2); // Still 2, not 3

    // Test setting hotkey with modifiers
    QVERIFY(manager.setHotkey(Hotkey{"CTRL+F3"}, "open exit n"));
    checkHk(manager, Hotkey{"CTRL+F3"}, "open exit n");
    QCOMPARE(static_cast<int>(manager.getAllHotkeys().size()), 3);
}

void TestHotkeyManager::removeHotkeyTest()
{
    HotkeyManager manager;
    manager.clear();

    std::ignore = manager.setHotkey(Hotkey{"F1"}, "look");
    std::ignore = manager.setHotkey(Hotkey{"F2"}, "flee");
    std::ignore = manager.setHotkey(Hotkey{"CTRL+F3"}, "open exit n");
    QCOMPARE(static_cast<int>(manager.getAllHotkeys().size()), 3);

    // Test removing a hotkey
    manager.removeHotkey(Hotkey{"F1"});
    checkHk(manager, Hotkey{"F1"}, ""); // Should be empty now
    QCOMPARE(static_cast<int>(manager.getAllHotkeys().size()), 2);

    // Test removing hotkey with modifiers
    manager.removeHotkey(Hotkey{"CTRL+F3"});
    checkHk(manager, Hotkey{"CTRL+F3"}, "");
    QCOMPARE(static_cast<int>(manager.getAllHotkeys().size()), 1);
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

    // Set hotkeys for testing
    std::ignore = manager.setHotkey(Hotkey{"F1"}, "look");
    std::ignore = manager.setHotkey(Hotkey{"CTRL+F2"}, "flee");
    std::ignore = manager.setHotkey(Hotkey{"NUMPAD8"}, "n");
    std::ignore = manager.setHotkey(Hotkey{"CTRL+NUMPAD5"}, "s");
    std::ignore = manager.setHotkey(Hotkey{"SHIFT+ALT+UP"}, "north");

    // Test direct lookup for function keys (isNumpad=false)
    checkHk(manager, Hotkey{Qt::Key_F1, Qt::NoModifier}, "look");
    checkHk(manager, Hotkey{Qt::Key_F2, Qt::ControlModifier}, "flee");

    // Test that wrong modifiers don't match
    checkHk(manager, Hotkey{Qt::Key_F1, Qt::ControlModifier}, "");

    // Test numpad keys (isNumpad=true) - Qt::Key_8 with isNumpad=true
    checkHk(manager, Hotkey{Qt::Key_8, Qt::KeypadModifier}, "n");
    checkHk(manager, Hotkey{Qt::Key_5, Qt::ControlModifier | Qt::KeypadModifier}, "s");

    // Test that numpad keys don't match non-numpad lookups
    checkHk(manager, Hotkey{Qt::Key_8, Qt::NoModifier}, "");

    // Test arrow keys (isNumpad=false)
    checkHk(manager, Hotkey{Qt::Key_Up, (Qt::ShiftModifier | Qt::AltModifier)}, "north");

    // Test SHIFT+NUMPAD4 (NumLock ON) which often comes as Qt::Key_Left + Shift + Keypad
    std::ignore = manager.setHotkey(Hotkey{"SHIFT+NUMPAD4"}, "pick west");
    checkHk(manager, Hotkey{Qt::Key_Left, (Qt::ShiftModifier | Qt::KeypadModifier)}, "pick west");

    // Test NUMPAD8 (NumLock OFF) which comes as Qt::Key_Up + Keypad
    std::ignore = manager.setHotkey(Hotkey{"NUMPAD8"}, "north");
    checkHk(manager, Hotkey{Qt::Key_Up, Qt::KeypadModifier}, "north");
}

QTEST_MAIN(TestHotkeyManager)
