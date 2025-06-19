// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2023 The MMapper Authors

#include "TestClientPage.h"
#include "../src/preferences/clientpage.h"
#include "../src/configuration/configuration.h"
#include "ui_clientpage.h" // For direct UI element access

#include <QtTest/QtTest>
#include <QSignalSpy>
#include <QFontComboBox> // For ui->fontComboBox
#include <QPushButton>
#include <QSpinBox>
#include <QCheckBox>
#include <QColor>
#include <QFont>

TestClientPage::TestClientPage() {}
TestClientPage::~TestClientPage() {}

void TestClientPage::initTestCase() {
    m_pristineDefaultConfig = std::make_unique<Configuration>(getConfig());
    m_pristineDefaultConfig->reset();
    QVERIFY(m_pristineDefaultConfig);
}
void TestClientPage::cleanupTestCase() {}

void TestClientPage::init() {
    m_clientPage = new ClientPage(nullptr);
    QMetaObject::invokeMethod(m_clientPage, "slot_loadConfig", Qt::DirectConnection);
}

void TestClientPage::cleanup() {
    delete m_clientPage;
    m_clientPage = nullptr;
}

void TestClientPage::testLoadSettings() {
    QVERIFY(m_clientPage);
    Ui::ClientPage *ui = m_clientPage->ui; // Assuming 'ui' is public or friend for tests
    QVERIFY(ui);

    const auto& clientSettings = getConfig().integratedClient;
    QFont currentFont;
    currentFont.fromString(clientSettings.getFont());
    // The UI uses a QPushButton for font, not QFontComboBox based on ClientPage.cpp
    // QCOMPARE(ui->fontComboBox->currentFont().toString(), currentFont.toString());
    // We can check the text of fontPushButton
    QFontInfo fi(currentFont);
    QString expectedFontButtonText = QString("%1 %2, %3").arg(fi.family()).arg(fi.styleName()).arg(fi.pointSize());
    QCOMPARE(ui->fontPushButton->text(), expectedFontButtonText);

    QCOMPARE(ui->columnsSpinBox->value(), clientSettings.getColumns());
    QCOMPARE(ui->clearInputCheckBox->isChecked(), clientSettings.getClearInputOnEnter());
    // Color button check is harder (icon/stylesheet), skip direct check here for brevity
}

void TestClientPage::testChangeFontSetting() {
    QVERIFY(m_clientPage);
    Ui::ClientPage *ui = m_clientPage->ui;
    QVERIFY(ui);

    QString initialFontStr = getConfig().integratedClient.getFont();
    QFont testFont("Arial", 12);
    if (testFont.toString() == initialFontStr) {
        testFont.setFamily("Courier New"); // Ensure different
    }

    // Simulate the action of slot_onChangeFont, which is triggered by fontPushButton
    // This slot opens a QFontDialog. We can't easily test that directly.
    // Instead, we'll test by setting the config and ensuring the UI updates via the monitor.
    // This is covered in testUiUpdatesOnExternalConfigChange.
    // For this test, let's verify the setter path if we directly call the slot after mocking dialog.
    // Or, more simply, call the setter and check config.

    setConfig().integratedClient.setFont(testFont.toString());
    QTest::qWait(50);

    QCOMPARE(getConfig().integratedClient.getFont(), testFont.toString());
    // UI update via monitor is tested in testUiUpdatesOnExternalConfigChange
}

void TestClientPage::testChangeColorSetting() { // e.g., foregroundColor
    QVERIFY(m_clientPage);
    Ui::ClientPage *ui = m_clientPage->ui;
    QVERIFY(ui);

    QColor initialColor = getConfig().integratedClient.getForegroundColor();
    QColor testColor = (initialColor == Qt::blue) ? Qt::red : Qt::blue;

    setConfig().integratedClient.setForegroundColor(testColor);
    QTest::qWait(50);

    QCOMPARE(getConfig().integratedClient.getForegroundColor(), testColor);
    // UI update (button icon) via monitor is tested in testUiUpdatesOnExternalConfigChange
}


void TestClientPage::testChangeIntSetting() { // e.g., columns
    QVERIFY(m_clientPage);
    Ui::ClientPage *ui = m_clientPage->ui;
    QVERIFY(ui);

    int initialVal = getConfig().integratedClient.getColumns();
    int testVal = initialVal + 5;

    ui->columnsSpinBox->setValue(testVal); // This triggers valueChanged -> slot -> setter
    QTest::qWait(50);

    QCOMPARE(getConfig().integratedClient.getColumns(), testVal);
}

void TestClientPage::testChangeBoolSetting() { // e.g., clearInputOnEnter
    QVERIFY(m_clientPage);
    Ui::ClientPage *ui = m_clientPage->ui;
    QVERIFY(ui);

    bool initialVal = getConfig().integratedClient.getClearInputOnEnter();
    ui->clearInputCheckBox->setChecked(!initialVal); // This triggers toggled -> lambda -> setter
    QTest::qWait(50);

    QCOMPARE(getConfig().integratedClient.getClearInputOnEnter(), !initialVal);
}

void TestClientPage::testSignalEmissionOnConfigChange() {
    QVERIFY(m_clientPage);
    Ui::ClientPage *ui = m_clientPage->ui;
    QVERIFY(ui);

    QSignalSpy spy(m_clientPage, &ClientPage::sig_clientSettingsChanged);
    QVERIFY(spy.isValid());

    // Change a boolean setting via UI
    bool initialClearVal = getConfig().integratedClient.getClearInputOnEnter();
    ui->clearInputCheckBox->setChecked(!initialClearVal);
    QTest::qWait(100);
    QCOMPARE(spy.count(), 1);

    spy.clear();
    // Change an int setting via UI
    int initialCols = getConfig().integratedClient.getColumns();
    ui->columnsSpinBox->setValue(initialCols + 1);
    QTest::qWait(100);
    QCOMPARE(spy.count(), 1);

    // Test programmatic change (simulating external change or complex slot)
    spy.clear();
    QColor initialColor = getConfig().integratedClient.getBackgroundColor();
    QColor testColor = (initialColor == Qt::darkCyan) ? Qt::darkMagenta : Qt::darkCyan;
    setConfig().integratedClient.setBackgroundColor(testColor); // Programmatic change
    QTest::qWait(100); // Allow monitor -> ClientPage callback -> emit
    QCOMPARE(spy.count(), 1);
}

void TestClientPage::testUiUpdatesOnExternalConfigChange() {
    QVERIFY(m_clientPage);
    Ui::ClientPage *ui = m_clientPage->ui; // Assuming 'ui' is public or friend
    QVERIFY(ui);

    // Test 1: Change boolean (clearInputOnEnter)
    bool originalBool = getConfig().integratedClient.getClearInputOnEnter();
    bool externalBoolValue = !originalBool;
    setConfig().integratedClient.setClearInputOnEnter(externalBoolValue);
    QTest::qWait(100);
    QCOMPARE(ui->clearInputCheckBox->isChecked(), externalBoolValue);

    // Test 2: Change integer (columns)
    int originalInt = getConfig().integratedClient.getColumns();
    int externalIntValue = originalInt + 10;
    setConfig().integratedClient.setColumns(externalIntValue);
    QTest::qWait(100);
    QCOMPARE(ui->columnsSpinBox->value(), externalIntValue);

    // Test 3: Change font
    QFont originalFontQt;
    originalFontQt.fromString(getConfig().integratedClient.getFont());
    QFont externalFontQt("Verdana", 15);
    if (externalFontQt.toString() == originalFontQt.toString()) externalFontQt.setFamily("Impact");

    setConfig().integratedClient.setFont(externalFontQt.toString());
    QTest::qWait(100);
    // Verify fontPushButton's text
    QFontInfo fi(externalFontQt);
    QString expectedFontButtonText = QString("%1 %2, %3").arg(fi.family()).arg(fi.styleName()).arg(fi.pointSize());
    QCOMPARE(ui->fontPushButton->text(), expectedFontButtonText);

    // Test 4: Change color (e.g., foregroundColor)
    QColor originalColor = getConfig().integratedClient.getForegroundColor();
    QColor externalColor = (originalColor == Qt::cyan) ? Qt::magenta : Qt::cyan;
    setConfig().integratedClient.setForegroundColor(externalColor);
    QTest::qWait(100);
    // To verify button color, we check the icon. ClientPage::updateFontAndColors updates it.
    // This requires access to the button's icon pixmap.
    QIcon icon = ui->fgColorPushButton->icon();
    QVERIFY(!icon.isNull());
    QPixmap pixmap = icon.pixmap(16, 16);
    QVERIFY(!pixmap.isNull());
    QCOMPARE(pixmap.toImage().pixelColor(0,0), externalColor);


    // Restore original values
    setConfig().integratedClient.setClearInputOnEnter(originalBool);
    setConfig().integratedClient.setColumns(originalInt);
    setConfig().integratedClient.setFont(originalFontQt.toString());
    setConfig().integratedClient.setForegroundColor(originalColor);
    QTest::qWait(100); // Allow final changes to settle
}

QTEST_MAIN(TestClientPage)
