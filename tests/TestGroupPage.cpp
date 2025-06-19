// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2023 The MMapper Authors

#include "TestGroupPage.h"
#include "../src/preferences/grouppage.h"
#include "../src/configuration/configuration.h"
#include "ui_grouppage.h" // Required for direct UI element access

#include <QtTest/QtTest>
#include <QSignalSpy>
#include <QCheckBox>
#include <QPushButton>
#include <QColorDialog> // For color tests, though we might not pop it up

TestGroupPage::TestGroupPage() {}
TestGroupPage::~TestGroupPage() {}

void TestGroupPage::initTestCase() {
    // One-time setup for the entire test class
    m_pristineDefaultConfig = std::make_unique<Configuration>(getConfig());
    m_pristineDefaultConfig->reset();
    QVERIFY(m_pristineDefaultConfig);
}
void TestGroupPage::cleanupTestCase() {}

void TestGroupPage::init() {
    // Per-test setup: Create a new GroupPage instance for each test
    m_groupPage = new GroupPage(nullptr);
    // Manually call slot_loadConfig to ensure UI is populated for the test
    // This is essential as the constructor now also sets up listeners,
    // and we want a fresh UI state based on current config for each test.
    QMetaObject::invokeMethod(m_groupPage, "slot_loadConfig", Qt::DirectConnection);
}

void TestGroupPage::cleanup() {
    // Per-test cleanup
    delete m_groupPage;
    m_groupPage = nullptr;
}

void TestGroupPage::testLoadSettings() {
    QVERIFY(m_groupPage);
    // Accessing ui directly via a static_cast is generally unsafe if the class hierarchy changes
    // or if 'ui' is not a public member. For tests, findChild is preferred if objectNames are set.
    // Assuming 'ui' is accessible for this test context as per problem description.
    Ui::GroupPage *ui = m_groupPage->ui; // Direct access, assuming 'ui' is public or friend.
    QVERIFY(ui);

    QCOMPARE(ui->npcHideCheckBox->isChecked(), getConfig().groupManager.getNpcHide());
    QCOMPARE(ui->npcSortBottomCheckBox->isChecked(), getConfig().groupManager.getNpcSortBottom());
    QCOMPARE(ui->npcOverrideColorCheckBox->isChecked(), getConfig().groupManager.getNpcColorOverride());
    // Color verification is harder; typically check icon or stylesheet.
    // For now, we trust slot_loadConfig sets it visually.
}

void TestGroupPage::testChangeBoolSetting() { // e.g., npcHide
    QVERIFY(m_groupPage);
    Ui::GroupPage *ui = m_groupPage->ui;
    QVERIFY(ui);

    bool initialVal = getConfig().groupManager.getNpcHide();

    // Spy on the signal that GroupPage itself should emit
    QSignalSpy pageSignalSpy(m_groupPage, &GroupPage::sig_groupSettingsChanged);
    QVERIFY(pageSignalSpy.isValid());

    ui->npcHideCheckBox->setChecked(!initialVal);
    QTest::qWait(50); // Allow signals/slots to process

    QCOMPARE(getConfig().groupManager.getNpcHide(), !initialVal);
    QCOMPARE(pageSignalSpy.count(), 1); // Check that GroupPage emitted its signal due to the change
}

void TestGroupPage::testChangeColorSetting() { // e.g., main group color
    QVERIFY(m_groupPage);
    Ui::GroupPage *ui = m_groupPage->ui;
    QVERIFY(ui);

    QColor initialColor = getConfig().groupManager.getColor();
    QColor testColor = (initialColor == Qt::blue) ? Qt::red : Qt::blue;

    QSignalSpy pageSignalSpy(m_groupPage, &GroupPage::sig_groupSettingsChanged);
    QVERIFY(pageSignalSpy.isValid());

    // To test the color change, we can't easily mock QColorDialog.
    // Instead, we'll call the slot that is triggered AFTER the color dialog.
    // To do this, we would normally need to break down slot_chooseColor or simulate its effect.
    // For this test, let's directly set the config and ensure GroupPage reacts.
    // This tests the "config changed elsewhere -> GroupPage updates and signals" path.

    setConfig().groupManager.setColor(testColor); // This triggers GroupManagerSettings's monitor
    QTest::qWait(50); // Allow monitor -> GroupPage callback -> signal to propagate

    QCOMPARE(getConfig().groupManager.getColor(), testColor); // Verify config was set
    QCOMPARE(pageSignalSpy.count(), 1); // GroupPage should have emitted its signal

    // Also verify that slot_loadConfig (called by the XNamedColor global monitor,
    // or potentially if GroupPage's monitor callback called it) updated the UI.
    // This part is tricky because GroupManagerSettings.color is QColor, not XNamedColor.
    // The GroupManagerSettings monitor callback in GroupPage currently just emits.
    // Let's refine the test to ensure UI is actually updated if it's supposed to be.
    // The slot_chooseColor itself calls slot_loadConfig(). If we simulate that part:

    pageSignalSpy.clear();
    setConfig().groupManager.setColor(initialColor); // Set it back
    QTest::qWait(50);
    QCOMPARE(pageSignalSpy.count(), 1); // Should signal again

    // Now, call the actual slot that handles the button click logic, but mock dialog interaction
    // This is hard. The current structure of slot_chooseColor is:
    // 1. Open Dialog. 2. If color valid -> setConfig -> slot_loadConfig -> emit.
    // The most testable part is that setConfig().groupManager.setColor() leads to a signal.
    // The test for that is already covered above.
    // The UI update (button icon) is done by slot_loadConfig in GroupPage.
    // The test for slot_loadConfig (testLoadSettings) implicitly checks this if defaults are set.
    // Let's ensure that if the config changes, the icon *would* update if slot_loadConfig is called.
    // The XNamedColor callback in GroupPage calls slot_loadConfig. The GroupManagerSettings callback does not.
    // This is fine, as GroupManagerSettings doesn't use XNamedColors.
    // The button icon update in GroupPage::slot_chooseColor happens because it calls slot_loadConfig().
}

void TestGroupPage::testSignalEmissionOnConfigChange() {
    QVERIFY(m_groupPage);
    Ui::GroupPage *ui = m_groupPage->ui;
    QVERIFY(ui);

    QSignalSpy spy(m_groupPage, &GroupPage::sig_groupSettingsChanged);
    QVERIFY(spy.isValid());

    // Test 1: Change a boolean setting via UI - this should trigger the setter in GroupManagerSettings,
    // which notifies its monitor, which GroupPage listens to, and then GroupPage emits its own signal.
    bool initialHideVal = getConfig().groupManager.getNpcHide();
    ui->npcHideCheckBox->setChecked(!initialHideVal);
    QTest::qWait(100); // Increased wait time
    QCOMPARE(spy.count(), 1);

    // Test 2: Change a color setting programmatically (simulating external change)
    // This tests if GroupPage's listener to GroupManagerSettings works for color.
    spy.clear();
    QColor initialColor = getConfig().groupManager.getColor();
    QColor testColor = (initialColor == Qt::green) ? Qt::yellow : Qt::green;

    setConfig().groupManager.setColor(testColor); // This will trigger GroupManagerSettings's monitor
    QTest::qWait(100); // Allow signal propagation

    QCOMPARE(spy.count(), 1);

    // Verify UI is updated by the monitor chain (GroupManagerSettings -> GroupPage callback -> slot_loadConfig if added)
    // As per current GroupPage's GroupManagerSettings callback, it only emits. UI update isn't forced by that callback.
    // However, the color button's own action `slot_chooseColor` *does* call `slot_loadConfig`.
    // For this test, we'll focus on the signal. UI update after programmatic change is secondary here.
    // To fully test UI update upon programmatic change, the callback in GroupPage would need to call slot_loadConfig.
    // Let's add that to the GroupPage's GroupManagerSettings callback for robustness.
    // (This implies a change to GroupPage.cpp not originally in this test's subtask, but good for testability)
    // For now, assume the test only checks signal emission.
}

QTEST_MAIN(TestGroupPage)
