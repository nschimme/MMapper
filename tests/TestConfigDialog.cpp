// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2023 The MMapper Authors

#include "TestConfigDialog.h"
#include "../src/preferences/configdialog.h"
#include "../src/preferences/developerpage.h" // For DeveloperPage
#include "../src/configuration/configuration.h"
#include "ui_configdialog.h" // Required for ui->listWidget
#include "ui_developerpage.h" // Required for ui->searchLineEdit and ui->settingsLayout

#include <QtTest/QtTest>
#include <QListWidget>
#include <QStackedWidget>
#include <QLineEdit>
#include <QCheckBox>
#include <QFormLayout>
#include <QLabel>
#include <memory> // For std::unique_ptr, already in .h but good practice for .cpp if directly used

// Mock functions needed by Configuration during tests
// Normally provided by main.cpp or other setup
// Ensure these are defined if configuration needs them during instantiation or access
// If setEnteredMain() is usually called from main, we might need to simulate that for config access.
// For simplicity, we assume Configuration can be instantiated and accessed.
// If not, this test setup might need to be more complex.

TestConfigDialog::TestConfigDialog() {}
TestConfigDialog::~TestConfigDialog() {}

void TestConfigDialog::initTestCase()
{
    // It's possible setEnteredMain() needs to be called for setConfig()/getConfig() to work.
    // If Configuration is complex to initialize outside the app, this might be tricky.
    // For now, assume it works or that necessary parts are self-contained.
    // setEnteredMain(); // Potentially needed

    m_configDialog = new ConfigDialog(nullptr);
    m_configDialog->show(); // Show is needed to trigger showEvent and potentially loadConfig if connected to it

    // Find DeveloperPage
    // The pagesWidget is directly accessible via ui->pagesWidget in ConfigDialog after setupUi.
    // However, the member m_pagesWidget in ConfigDialog is what's actually used.
    // Let's try to find it by object name if set, or rely on its type.
    // ConfigDialog uses m_pagesWidget and adds it to ui->pagesScrollArea.
    // The QStackedWidget itself is not named "pagesWidget" but is the m_pagesWidget.
    // We can find the scroll area and get its widget.
    QScrollArea *scrollArea = m_configDialog->findChild<QScrollArea*>("pagesScrollArea");
    QVERIFY(scrollArea);
    QStackedWidget *stackedWidget = qobject_cast<QStackedWidget*>(scrollArea->widget());
    QVERIFY(stackedWidget);


    for (int i = 0; i < stackedWidget->count(); ++i) {
        DeveloperPage *page = qobject_cast<DeveloperPage*>(stackedWidget->widget(i));
        if (page) {
            m_developerPage = page;
            break;
        }
    }
    QVERIFY(m_developerPage);

    // Trigger loading of config for the page manually if not done by show() or if sig_loadConfig isn't automatically emitted/connected yet for tests
    // In a real app, sig_loadConfig is emitted. Here we ensure it's called.
    QMetaObject::invokeMethod(m_developerPage, "slot_loadConfig", Qt::DirectConnection);


    m_pageListWidget = m_configDialog->findChild<QListWidget*>("contentsWidget"); // Name is contentsWidget in ui
    QVERIFY(m_pageListWidget);

    m_searchLineEdit = m_developerPage->findChild<QLineEdit*>("searchLineEdit");
    QVERIFY(m_searchLineEdit);

    // Create a pristine configuration to compare against for defaults
    m_pristineDefaultConfig = std::make_unique<Configuration>(getConfig()); // Create a copy from the main config
    m_pristineDefaultConfig->reset(); // Reset it to its default state.
                                     // This assumes reset() correctly establishes initial defaults.
    QVERIFY(m_pristineDefaultConfig);
}

void TestConfigDialog::cleanupTestCase()
{
    if (m_configDialog) {
        m_configDialog->close(); // Close it
        delete m_configDialog;
        m_configDialog = nullptr;
    }
    // m_developerPage is a child of m_configDialog, so it's deleted with it.
}

void TestConfigDialog::testDeveloperPageExists()
{
    QVERIFY(m_developerPage != nullptr);
    bool foundInList = false;
    for (int i = 0; i < m_pageListWidget->count(); ++i) {
        if (m_pageListWidget->item(i)->text() == "Developer") {
            foundInList = true;
            // Optionally, select it to make it the current page
            // m_pageListWidget->setCurrentItem(m_pageListWidget->item(i));
            // QVERIFY(qobject_cast<DeveloperPage*>(m_configDialog->findChild<QStackedWidget*>("pagesWidget")->currentWidget()) != nullptr);
            break;
        }
    }
    QVERIFY(foundInList);
}

void TestConfigDialog::testDeveloperPagePopulation()
{
    QVERIFY(m_developerPage != nullptr);
    // Access settingsLayout from DeveloperPage's UI
    // QVBoxLayout* settingsLayout = m_developerPage->findChild<QVBoxLayout*>("settingsLayout");
    // The settingsLayout is a direct member of ui in DeveloperPage (ui->settingsLayout)
    QVERIFY(m_developerPage->ui->settingsLayout);


    // After slot_loadConfig, settingsLayout should contain a QFormLayout,
    // which in turn contains widgets.
    QVERIFY(m_developerPage->ui->settingsLayout->count() > 0);
    QFormLayout *formLayout = qobject_cast<QFormLayout*>(m_developerPage->ui->settingsLayout->itemAt(0)->layout());
    QVERIFY(formLayout);
    QVERIFY(formLayout->rowCount() > 0); // Check if any properties were added
}

void TestConfigDialog::testDeveloperPageSearch()
{
    QVERIFY(m_developerPage != nullptr);
    QVERIFY(m_searchLineEdit != nullptr);

    QFormLayout *formLayout = qobject_cast<QFormLayout*>(m_developerPage->ui->settingsLayout->itemAt(0)->layout());
    QVERIFY(formLayout && formLayout->rowCount() > 0);

    // Find a specific known boolean property for testing search, e.g., "alwaysOnTop" from GeneralSettings
    QString knownPropertyName = "alwaysOnTop";
    QLabel* testLabel = nullptr;
    QWidget* testWidget = nullptr;
    int testRow = -1;

    for(int i=0; i < formLayout->rowCount(); ++i) {
        QLabel* label = qobject_cast<QLabel*>(formLayout->itemAt(i, QFormLayout::LabelRole)->widget());
        if (label && label->text().startsWith(knownPropertyName)) {
            testLabel = label;
            testWidget = formLayout->itemAt(i, QFormLayout::FieldRole)->widget();
            testRow = i;
            break;
        }
    }
    QVERIFY2(testLabel, "Could not find a known property (e.g., alwaysOnTop) to test search. Check property name and if it's populated.");
    QVERIFY(testWidget);
    QVERIFY(testRow != -1);


    // Test 1: Search for the known property name
    m_searchLineEdit->setText(knownPropertyName);
    QTest::qWait(100);
    QVERIFY(formLayout->isRowVisible(testRow));
    QVERIFY(testLabel->isVisible()); // Label visibility should be controlled by row visibility now
    QVERIFY(testWidget->isVisible());


    // Test 2: Search for something that should hide it
    m_searchLineEdit->setText("ThIsShOuLdNoTmAtChAnYtHiNg");
    QTest::qWait(100);
    QVERIFY(!formLayout->isRowVisible(testRow));
    // Depending on implementation, individual widgets might also be set invisible
    // QVERIFY(!testLabel->isVisible());
    // QVERIFY(!testWidget->isVisible());


    // Test 3: Clear search - should be visible again
    m_searchLineEdit->clear();
    QTest::qWait(100);
    QVERIFY(formLayout->isRowVisible(testRow));
    QVERIFY(testLabel->isVisible());
    QVERIFY(testWidget->isVisible());
}

void TestConfigDialog::testDeveloperPageSettingChange()
{
    QVERIFY(m_developerPage != nullptr);

    QFormLayout *formLayout = qobject_cast<QFormLayout*>(m_developerPage->ui->settingsLayout->itemAt(0)->layout());
    QVERIFY(formLayout);

    QString targetSettingName = "alwaysOnTop";
    QCheckBox *targetCheckBox = nullptr;

    for (int i = 0; i < formLayout->rowCount(); ++i) {
        QLabel *label = qobject_cast<QLabel*>(formLayout->itemAt(i, QFormLayout::LabelRole)->widget());
        if (label && label->text().startsWith(targetSettingName)) {
            targetCheckBox = qobject_cast<QCheckBox*>(formLayout->itemAt(i, QFormLayout::FieldRole)->widget());
            break;
        }
    }

    QVERIFY2(targetCheckBox, "Could not find QCheckBox for 'alwaysOnTop' setting. Check property name and type.");

    // Ensure getConfig() and setConfig() are working as expected for tests
    // May need to call setEnteredMain() if Configuration relies on it.
    // Configuration& currentConfig = setConfig(); // to ensure config is loaded if lazy

    bool initialValue = getConfig().general.alwaysOnTop;

    targetCheckBox->setChecked(!initialValue);
    QTest::qWait(50);

    QCOMPARE(getConfig().general.alwaysOnTop, !initialValue);

    // Reset to original value
    targetCheckBox->setChecked(initialValue);
    QTest::qWait(50);
    QCOMPARE(getConfig().general.alwaysOnTop, initialValue);
}

void TestConfigDialog::testDeveloperPageGraphicsSignal()
{
    QVERIFY(m_developerPage != nullptr);

    // Let's find the QCheckBox for "drawDoorNames" as it's a known graphics bool.
    // Accessing ui members directly like m_developerPage->ui->... is fine for tests if needed,
    // but findChild is also robust if object names are set or types are unique.
    // Here, we navigate through the known structure.
    QFormLayout *formLayout = qobject_cast<QFormLayout*>(m_developerPage->ui->settingsLayout->itemAt(0)->layout());
    QVERIFY(formLayout);

    QString targetSettingName = "drawDoorNames"; // From Configuration::CanvasSettings
    QCheckBox *targetCheckBox = nullptr;

    for (int i = 0; i < formLayout->rowCount(); ++i) {
        QLabel *label = qobject_cast<QLabel*>(formLayout->itemAt(i, QFormLayout::LabelRole)->widget());
        if (label && label->text().startsWith(targetSettingName)) {
            targetCheckBox = qobject_cast<QCheckBox*>(formLayout->itemAt(i, QFormLayout::FieldRole)->widget());
            break;
        }
    }
    QVERIFY2(targetCheckBox, "Could not find QCheckBox for 'drawDoorNames' to test signal emission. Check property name and type in DeveloperPage::populatePage.");

    QSignalSpy spy(m_developerPage, &DeveloperPage::sig_graphicsSettingsChanged);
    QVERIFY(spy.isValid()); // Check if spy is correctly connected

    // Change the setting
    bool initialValue = getConfig().canvas.drawDoorNames; // Assuming 'canvas' is the member name for CanvasSettings in Configuration
    targetCheckBox->setChecked(!initialValue);
    QTest::qWait(50); // Allow signal to propagate if needed (usually not for direct connections)

    // Check that the signal was emitted
    QCOMPARE(spy.count(), 1);

    // Change it back and check signal again
    targetCheckBox->setChecked(initialValue);
    QTest::qWait(50);
    QCOMPARE(spy.count(), 2); // Emitted again for the second change
}

void TestConfigDialog::testDeveloperPageResetToDefault()
{
    QVERIFY(m_developerPage);
    QVERIFY(m_pristineDefaultConfig); // Ensure our source of defaults is available

    // --- Test Case 1: Boolean property (e.g., "drawDoorNames") ---
    QString boolPropertyName = "drawDoorNames";
    QCheckBox* boolEditor = nullptr;

    QFormLayout *formLayout = qobject_cast<QFormLayout*>(m_developerPage->ui->settingsLayout->itemAt(0)->layout());
    QVERIFY(formLayout);

    for (int i = 0; i < formLayout->rowCount(); ++i) {
        QLabel *labelWidget = qobject_cast<QLabel*>(formLayout->itemAt(i, QFormLayout::LabelRole)->widget());
        if (labelWidget && labelWidget->text().startsWith(boolPropertyName)) {
            boolEditor = qobject_cast<QCheckBox*>(formLayout->itemAt(i, QFormLayout::FieldRole)->widget());
            break;
        }
    }
    QVERIFY2(boolEditor, "Could not find editor for boolean property 'drawDoorNames' to test reset.");

    const QMetaObject* defaultMetaObj = m_pristineDefaultConfig->staticMetaObject;
    int propIdx = defaultMetaObj->indexOfProperty(boolPropertyName.toUtf8().constData());
    QVERIFY(propIdx != -1);
    QVariant expectedDefaultBool = defaultMetaObj->property(propIdx).read(m_pristineDefaultConfig.get());
    QVERIFY(expectedDefaultBool.isValid() && expectedDefaultBool.typeId() == QMetaType::Bool);

    // 1. Change the live setting away from default
    // bool initialLiveValue = getConfig().canvas.drawDoorNames; // Not needed directly
    bool valueToSet = !expectedDefaultBool.toBool();
    if (boolEditor->isChecked() == valueToSet) { // If current UI state is already what we want to set
        boolEditor->setChecked(!valueToSet); // Change it to something else first
        QTest::qWait(50);
    }
    boolEditor->setChecked(valueToSet);
    QTest::qWait(50);
    QCOMPARE(getConfig().canvas.drawDoorNames, valueToSet);

    // 2. Trigger reset for this property
    QMetaObject::invokeMethod(m_developerPage, "setProperty", Qt::DirectConnection,
                              Q_ARG(QString, "m_contextMenuPropertyName"), Q_ARG(QVariant, QVariant(boolPropertyName)));

    QSignalSpy graphicsSpy(m_developerPage, &DeveloperPage::sig_graphicsSettingsChanged);
    QVERIFY(graphicsSpy.isValid());

    QMetaObject::invokeMethod(m_developerPage, "onResetToDefaultTriggered", Qt::DirectConnection);
    QTest::qWait(50);

    // 3. Verify live config is reset to default
    QCOMPARE(getConfig().canvas.drawDoorNames, expectedDefaultBool.toBool());

    // 4. Verify UI widget is updated
    QCOMPARE(boolEditor->isChecked(), expectedDefaultBool.toBool());

    // 5. Verify signal was emitted (drawDoorNames is a graphics prop)
    QCOMPARE(graphicsSpy.count(), 1); // Reset action should emit it once.

    // --- Test Case 2: String property (e.g., "resourcesDirectory") ---
    QString stringPropertyName = "resourcesDirectory";
    QLineEdit* stringEditor = nullptr;
    for (int i = 0; i < formLayout->rowCount(); ++i) {
        QLabel *labelWidget = qobject_cast<QLabel*>(formLayout->itemAt(i, QFormLayout::LabelRole)->widget());
        if (labelWidget && labelWidget->text().startsWith(stringPropertyName)) {
            stringEditor = qobject_cast<QLineEdit*>(formLayout->itemAt(i, QFormLayout::FieldRole)->widget());
            break;
        }
    }
    QVERIFY2(stringEditor, "Could not find editor for string property 'resourcesDirectory' to test reset.");

    propIdx = defaultMetaObj->indexOfProperty(stringPropertyName.toUtf8().constData());
    QVERIFY(propIdx != -1);
    QVariant expectedDefaultString = defaultMetaObj->property(propIdx).read(m_pristineDefaultConfig.get());
    QVERIFY(expectedDefaultString.isValid() && expectedDefaultString.typeId() == QMetaType::QString);

    // 1. Change live setting
    QString testStringValue = "test/path/for/reset";
    if (testStringValue == expectedDefaultString.toString()) testStringValue = "/another/test/path";
    if (stringEditor->text() == testStringValue) { // If current UI state is already what we want to set
        stringEditor->setText(testStringValue + "_temp"); // Change it to something else first
        QTest::qWait(50);
    }

    stringEditor->setText(testStringValue);
    QTest::qWait(50);
    QCOMPARE(getConfig().canvas.resourcesDirectory, testStringValue);

    // 2. Trigger reset
    QMetaObject::invokeMethod(m_developerPage, "setProperty", Qt::DirectConnection,
                              Q_ARG(QString, "m_contextMenuPropertyName"), Q_ARG(QVariant, QVariant(stringPropertyName)));
    graphicsSpy.clear();
    QMetaObject::invokeMethod(m_developerPage, "onResetToDefaultTriggered", Qt::DirectConnection);
    QTest::qWait(50);

    // 3. Verify live config reset
    QCOMPARE(getConfig().canvas.resourcesDirectory, expectedDefaultString.toString());

    // 4. Verify UI update
    QCOMPARE(stringEditor->text(), expectedDefaultString.toString());

    // 5. Verify signal (resourcesDirectory is a graphics prop)
    QCOMPARE(graphicsSpy.count(), 1);
}

QTEST_MAIN(TestConfigDialog)
