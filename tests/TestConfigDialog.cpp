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

QTEST_MAIN(TestConfigDialog)
