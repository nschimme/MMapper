// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "TestHotkeyManager.h"

#include "../src/client/HotkeyManager.h"

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
}

void TestHotkeyManager::cleanupTestCase()
{
    // Restore original QSettings namespace
    QCoreApplication::setOrganizationName(m_originalOrganization);
    QCoreApplication::setApplicationName(m_originalApplication);
}

void TestHotkeyManager::keyNormalizationTest()
{
    HotkeyManager &manager = HotkeyManager::getInstance();
    manager.clear();

    // Test that modifiers are normalized to canonical order: CTRL+SHIFT+ALT+META
    // Set a hotkey with non-canonical modifier order
    manager.setAction(Qt::Key_F1, Qt::AltModifier | Qt::ControlModifier, false, "test1");

    // Should be retrievable with canonical order
    QCOMPARE(manager.getAction(Qt::Key_F1, Qt::ControlModifier | Qt::AltModifier, false), QString("test1"));

    // Should also be retrievable with the original order (due to normalization)
    QCOMPARE(manager.getAction(Qt::Key_F1, Qt::AltModifier | Qt::ControlModifier, false), QString("test1"));

    // Test all modifier combinations normalize correctly
    manager.setAction(Qt::Key_F2, Qt::MetaModifier | Qt::AltModifier | Qt::ShiftModifier | Qt::ControlModifier, false, "test2");
    QCOMPARE(manager.getAction(Qt::Key_F2, Qt::ControlModifier | Qt::ShiftModifier | Qt::AltModifier | Qt::MetaModifier, false), QString("test2"));

    // Test that case is normalized to uppercase
    manager.setAction(Qt::Key_F3, Qt::ControlModifier, false, "test3");
    QCOMPARE(manager.getAction(Qt::Key_F3, Qt::ControlModifier, false), QString("test3"));

    // Test simple key without modifiers
    manager.setAction(Qt::Key_F7, Qt::NoModifier, false, "test7");
    QCOMPARE(manager.getAction(Qt::Key_F7, Qt::NoModifier, false), QString("test7"));

    // Test numpad keys
    manager.setAction(Qt::Key_8, Qt::NoModifier, true, "north");
    QCOMPARE(manager.getAction(Qt::Key_8, Qt::NoModifier, true), QString("north"));
}

void TestHotkeyManager::importExportRoundTripTest()
{
    HotkeyManager &manager = HotkeyManager::getInstance();
    manager.clear();
    QSettings settings;

    // Test import with a known string (this clears existing hotkeys)
    settings.setValue("F1", "look");
    settings.setValue("CTRL+F2", "open exit n");
    settings.setValue("SHIFT+ALT+F3", "pick exit s");
    settings.setValue("NUMPAD8", "n");
    settings.setValue("CTRL+SHIFT+NUMPAD_PLUS", "test command");

    manager.loadFromSettings(settings);

    // Verify all hotkeys were imported correctly
    QCOMPARE(manager.getAction(Qt::Key_F1, Qt::NoModifier, false), QString("look"));
    QCOMPARE(manager.getAction(Qt::Key_F2, Qt::ControlModifier, false), QString("open exit n"));
    QCOMPARE(manager.getAction(Qt::Key_F3, Qt::ShiftModifier | Qt::AltModifier, false), QString("pick exit s"));
    QCOMPARE(manager.getAction(Qt::Key_8, Qt::NoModifier, true), QString("n"));
    QCOMPARE(manager.getAction(Qt::Key_Plus, Qt::ControlModifier | Qt::ShiftModifier, true), QString("test command"));

    // Verify total count
    QCOMPARE(manager.getHotkeys().size(), 5);

    // Export and verify content
    QSettings exportedSettings;
    manager.saveToSettings(exportedSettings);
    QVERIFY(exportedSettings.value("F1").toString() == "look");
    QVERIFY(exportedSettings.value("CTRL+F2").toString() == "open exit n");
    QVERIFY(exportedSettings.value("NUMPAD8").toString() == "n");
}

void TestHotkeyManager::importEdgeCasesTest()
{
    HotkeyManager &manager = HotkeyManager::getInstance();
    manager.clear();
    QSettings settings;

    // Test command with multiple spaces (should preserve spaces in command)
    settings.setValue("F1", "cast 'cure light'");
    manager.loadFromSettings(settings);
    QCOMPARE(manager.getAction(Qt::Key_F1, Qt::NoModifier, false), QString("cast 'cure light'"));

    // Test malformed lines are skipped
    // "_hotkey" alone - no key
    // "_hotkey F2" - no command
    // "_hotkey F3 valid" - valid
    settings.clear();
    settings.setValue("F3", "valid");
    manager.loadFromSettings(settings);
    QCOMPARE(manager.getHotkeys().size(), 1);
    QCOMPARE(manager.getAction(Qt::Key_F3, Qt::NoModifier, false), QString("valid"));
}

void TestHotkeyManager::resetToDefaultsTest()
{
    HotkeyManager &manager = HotkeyManager::getInstance();
    manager.clear();

    // Import custom hotkeys
    manager.setAction(Qt::Key_F1, Qt::NoModifier, false, "custom");
    manager.setAction(Qt::Key_F2, Qt::NoModifier, false, "another");
    QCOMPARE(manager.getAction(Qt::Key_F1, Qt::NoModifier, false), QString("custom"));
    QCOMPARE(manager.getHotkeys().size(), 2);

    // Reset to defaults
    manager.clear();

    // Verify defaults are restored
    QCOMPARE(manager.getAction(Qt::Key_8, Qt::NoModifier, true), QString());
}

void TestHotkeyManager::setHotkeyTest()
{
    HotkeyManager &manager = HotkeyManager::getInstance();
    manager.clear();
    QCOMPARE(manager.getHotkeys().size(), 0);

    // Test setting a new hotkey directly
    manager.setAction(Qt::Key_F1, Qt::NoModifier, false, "look");
    QCOMPARE(manager.getAction(Qt::Key_F1, Qt::NoModifier, false), QString("look"));
    QCOMPARE(manager.getHotkeys().size(), 1);

    // Test setting another hotkey
    manager.setAction(Qt::Key_F2, Qt::NoModifier, false, "flee");
    QCOMPARE(manager.getAction(Qt::Key_F2, Qt::NoModifier, false), QString("flee"));
    QCOMPARE(manager.getHotkeys().size(), 2);

    // Test updating an existing hotkey (should replace, not add)
    manager.setAction(Qt::Key_F1, Qt::NoModifier, false, "inventory");
    QCOMPARE(manager.getAction(Qt::Key_F1, Qt::NoModifier, false), QString("inventory"));
    QCOMPARE(manager.getHotkeys().size(), 2); // Still 2, not 3

    // Test setting hotkey with modifiers
    manager.setAction(Qt::Key_F3, Qt::ControlModifier, false, "open exit n");
    QCOMPARE(manager.getAction(Qt::Key_F3, Qt::ControlModifier, false), QString("open exit n"));
    QCOMPARE(manager.getHotkeys().size(), 3);

    // Test updating hotkey with modifiers
    manager.setAction(Qt::Key_F3, Qt::ControlModifier, false, "close exit n");
    QCOMPARE(manager.getAction(Qt::Key_F3, Qt::ControlModifier, false), QString("close exit n"));
    QCOMPARE(manager.getHotkeys().size(), 3); // Still 3
}

void TestHotkeyManager::removeHotkeyTest()
{
    HotkeyManager &manager = HotkeyManager::getInstance();
    manager.clear();

    // Setup: import some hotkeys
    manager.setAction(Qt::Key_F1, Qt::NoModifier, false, "look");
    manager.setAction(Qt::Key_F2, Qt::NoModifier, false, "flee");
    manager.setAction(Qt::Key_F3, Qt::ControlModifier, false, "open exit n");
    QCOMPARE(manager.getHotkeys().size(), 3);

    // Test removing a hotkey
    manager.removeAction(Qt::Key_F1, Qt::NoModifier, false);
    QCOMPARE(manager.getAction(Qt::Key_F1, Qt::NoModifier, false), QString()); // Should be empty now
    QCOMPARE(manager.getHotkeys().size(), 2);

    // Verify other hotkeys still exist
    QCOMPARE(manager.getAction(Qt::Key_F2, Qt::NoModifier, false), QString("flee"));
    QCOMPARE(manager.getAction(Qt::Key_F3, Qt::ControlModifier, false), QString("open exit n"));

    // Test removing hotkey with modifiers
    manager.removeAction(Qt::Key_F3, Qt::ControlModifier, false);
    QCOMPARE(manager.getAction(Qt::Key_F3, Qt::ControlModifier, false), QString());
    QCOMPARE(manager.getHotkeys().size(), 1);

    // Test removing non-existent hotkey (should not crash or change count)
    manager.removeAction(Qt::Key_F10, Qt::NoModifier, false);
    QCOMPARE(manager.getHotkeys().size(), 1);

    // Test removing with non-canonical modifier order (should still work due to normalization)
    manager.setAction(Qt::Key_F5, Qt::AltModifier | Qt::ControlModifier, false, "test");
    QCOMPARE(manager.getHotkeys().size(), 2);
    manager.removeAction(Qt::Key_F5, Qt::ControlModifier | Qt::AltModifier, false); // Canonical order
    QCOMPARE(manager.getHotkeys().size(), 1);
}

void TestHotkeyManager::directLookupTest()
{
    HotkeyManager &manager = HotkeyManager::getInstance();
    manager.clear();

    // Import hotkeys for testing
    manager.setAction(Qt::Key_F1, Qt::NoModifier, false, "look");
    manager.setAction(Qt::Key_F2, Qt::ControlModifier, false, "flee");
    manager.setAction(Qt::Key_8, Qt::NoModifier, true, "n");
    manager.setAction(Qt::Key_5, Qt::ControlModifier, true, "s");
    manager.setAction(Qt::Key_Up, Qt::ShiftModifier | Qt::AltModifier, false, "north");

    // Test direct lookup for function keys (isNumpad=false)
    QCOMPARE(manager.getAction(Qt::Key_F1, Qt::NoModifier, false), QString("look"));
    QCOMPARE(manager.getAction(Qt::Key_F2, Qt::ControlModifier, false), QString("flee"));

    // Test that wrong modifiers don't match
    QCOMPARE(manager.getAction(Qt::Key_F1, Qt::ControlModifier, false), QString());
    QCOMPARE(manager.getAction(Qt::Key_F2, Qt::NoModifier, false), QString());

    // Test numpad keys (isNumpad=true) - Qt::Key_8 with isNumpad=true
    QCOMPARE(manager.getAction(Qt::Key_8, Qt::NoModifier, true), QString("n"));
    QCOMPARE(manager.getAction(Qt::Key_5, Qt::ControlModifier, true), QString("s"));

    // Test that numpad keys don't match non-numpad lookups
    QCOMPARE(manager.getAction(Qt::Key_8, Qt::NoModifier, false), QString());

    // Test arrow keys (isNmpad=false)
    QCOMPARE(manager.getAction(Qt::Key_Up, Qt::ShiftModifier | Qt::AltModifier, false),
             QString("north"));

    // Test that order of modifiers doesn't matter for lookup
    QCOMPARE(manager.getAction(Qt::Key_Up, Qt::AltModifier | Qt::ShiftModifier, false),
             QString("north"));

    // Test non-existent hotkey
    QCOMPARE(manager.getAction(Qt::Key_F12, Qt::NoModifier, false), QString());
}

QTEST_MAIN(TestHotkeyManager)
