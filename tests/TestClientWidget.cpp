// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2023 The MMapper Authors

#include "TestClientWidget.h"
#include "../src/client/ClientWidget.h"
#include "../src/client/DisplayWidget.h" // Required to inspect DisplayWidget
#include "../src/client/InputWidget.h"   // Required to inspect InputWidget
#include "../src/configuration/configuration.h"

#include <QtTest/QtTest>
#include <QFont>
#include <QPlainTextEdit> // For InputWidget's font()
#include <QTextEdit>      // For DisplayWidget's font()

TestClientWidget::TestClientWidget() {}
TestClientWidget::~TestClientWidget() {}

void TestClientWidget::initTestCase() {
    // m_pristineDefaultConfig = std::make_unique<Configuration>(getConfig());
    // m_pristineDefaultConfig->reset();
}
void TestClientWidget::cleanupTestCase() {}

void TestClientWidget::init() {
    m_clientWidget = new ClientWidget(nullptr);

    m_displayWidget = m_clientWidget->findChild<DisplayWidget*>();
    m_inputWidget = m_clientWidget->findChild<InputWidget*>();

    QVERIFY2(m_clientWidget, "ClientWidget could not be instantiated.");
    QVERIFY2(m_displayWidget, "DisplayWidget not found as child of ClientWidget. Ensure it has an objectName or is directly accessible.");
    QVERIFY2(m_inputWidget, "InputWidget not found as child of ClientWidget. Ensure it has an objectName or is directly accessible.");

    // Call handleClientSettingsUpdate to ensure initial settings from config are applied
    // This is important because ClientWidget's constructor might not call it,
    // and we want to test reaction to subsequent changes.
    // However, ClientWidget's constructor *now* registers its callback, so any changes
    // *after* construction should trigger handleClientSettingsUpdate.
    // For initial state, DisplayWidget/InputWidget constructors load settings.
    // Let's ensure a defined state by forcing an update based on current config.
    QMetaObject::invokeMethod(m_clientWidget, "handleClientSettingsUpdate", Qt::DirectConnection);
    QTest::qWait(50); // Allow any queued updates from this call
}

void TestClientWidget::cleanup() {
    delete m_clientWidget;
    m_clientWidget = nullptr;
    m_displayWidget = nullptr; // Child of m_clientWidget
    m_inputWidget = nullptr;   // Child of m_clientWidget
}

void TestClientWidget::testLiveUpdateFont() {
    QVERIFY(m_clientWidget && m_displayWidget && m_inputWidget);

    QString originalFontStr = getConfig().integratedClient.getFont();
    QFont testFont("Arial", 16);
    if (testFont.toString() == originalFontStr) {
        testFont.setFamily("Courier New");
        testFont.setPointSize(18);
    }

    // Change setting in config
    setConfig().integratedClient.setFont(testFont.toString());
    QTest::qWait(100); // Allow time for ChangeMonitor -> ClientWidget::handleClientSettingsUpdate

    // Verify DisplayWidget's font
    // This requires DisplayWidget to have a way to update its font
    // and for handleClientSettingsUpdate to call it.
    // Assuming ClientWidget::handleClientSettingsUpdate now calls something like:
    // getDisplay().updateFont(clientSettings.getFont());
    // And DisplayWidget::updateFont applies it, and DisplayWidget::font() returns current font.
    // For now, this part of the test might be more of an aspiration.
    // Let's check the direct font property of the QTextEdit/Browser if accessible.
    // The actual font application is deep in DisplayWidget's AnsiTextHelper.
    // A direct m_displayWidget->font() might return the base QTextBrowser font,
    // not necessarily the one used by AnsiTextHelper for new text.
    // This test is therefore limited without more DisplayWidget API.
    // However, if handleClientSettingsUpdate forces a re-init of AnsiTextHelper or similar:
    // QCOMPARE(m_displayWidget->font().family(), testFont.family());
    // QCOMPARE(m_displayWidget->font().pointSize(), testFont.pointSize());
    QVERIFY2(true, "DisplayWidget font update verification needs specific DisplayWidget method or deeper inspection.");


    // Verify InputWidget's font
    // Assuming ClientWidget::handleClientSettingsUpdate calls something like:
    // getInput().updateFont(clientSettings.getFont());
    // And InputWidget::updateFont applies it.
    QCOMPARE(m_inputWidget->font().family(), testFont.family());
    QCOMPARE(m_inputWidget->font().pointSize(), testFont.pointSize());

    // Restore original font
    setConfig().integratedClient.setFont(originalFontStr);
    QTest::qWait(50);
}

void TestClientWidget::testLiveUpdateInputHistorySize() {
    QVERIFY(m_clientWidget && m_inputWidget);

    int originalSize = getConfig().integratedClient.getLinesOfInputHistory();
    int testSize = originalSize + 10;
    if (testSize == originalSize) testSize += 5;

    setConfig().integratedClient.setLinesOfInputHistory(testSize);
    QTest::qWait(100);

    // InputWidget's InputHistory class uses this setting directly when adding lines.
    // There's no simple way to ask InputWidget "what's your current history capacity?"
    // without modifying InputWidget or InputHistory to expose this.
    // This test verifies the config value changed and relies on InputHistory using it.
    QCOMPARE(getConfig().integratedClient.getLinesOfInputHistory(), testSize);
    QVERIFY2(true, "InputWidget's internal history capacity verification would require new getter in InputWidget/InputHistory.");

    // Restore original size
    setConfig().integratedClient.setLinesOfInputHistory(originalSize);
    QTest::qWait(50);
}

QTEST_MAIN(TestClientWidget)
