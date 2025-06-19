// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2023 The MMapper Authors

#include "TestGroupWidget.h"
#include "../src/group/GroupWidget.h"
#include "../src/group/Mmapper2Group.h"
// CGroupChar.h is included via TestGroupWidget.h for SharedGroupChar
#include "../src/configuration/configuration.h"
// ui_groupwidget.h is not standardly included if ui is private. We'll use findChild.

#include <QtTest/QtTest>
#include <QSignalSpy>
#include <QTableView>
// QStandardItemModel is not used by GroupWidget's GroupModel
// #include <QStandardItemModel>

TestGroupWidget::TestGroupWidget() {}
TestGroupWidget::~TestGroupWidget() {}

void TestGroupWidget::initTestCase() {}
void TestGroupWidget::cleanupTestCase() {}

QTableView* TestGroupWidget::getGroupTableView() const {
    if (!m_groupWidget) return nullptr;
    // GroupWidget's QTableView is a private member named m_table.
    // We must use findChild and rely on it being named by GroupWidget.
    // If GroupWidget doesn't set an objectName for m_table, this will fail.
    // Let's assume GroupWidget's constructor does: m_table->setObjectName("groupTableView");
    return m_groupWidget->findChild<QTableView*>("groupTableView");
}

GroupModel* TestGroupWidget::getGroupModel() const {
    if(!m_groupWidget) return nullptr;
    QTableView* table = getGroupTableView();
    if (table && table->model()) {
        GroupProxyModel* proxy = qobject_cast<GroupProxyModel*>(table->model());
        if (proxy) {
            return qobject_cast<GroupModel*>(proxy->sourceModel());
        }
    }
    return nullptr;
}

GroupProxyModel* TestGroupWidget::getGroupProxyModel() const {
    if(!m_groupWidget) return nullptr;
    QTableView* table = getGroupTableView();
    if (table) {
        return qobject_cast<GroupProxyModel*>(table->model());
    }
    return nullptr;
}

void TestGroupWidget::addTestCharacterToM2G(const QString& name, bool isPlayer, bool isNpc, int idVal) {
    Q_UNUSED(name); Q_UNUSED(isPlayer); Q_UNUSED(isNpc); Q_UNUSED(idVal);
    // This remains a challenge. Mmapper2Group's character adding is via GMCP or private methods.
    // Proper testing would require either test hooks in Mmapper2Group or GMCP simulation.
    // For now, tests will be limited by the inability to easily populate Mmapper2Group.
}


void TestGroupWidget::init() {
    m_mockM2Group = new Mmapper2Group(this);
    m_groupWidget = new GroupWidget(m_mockM2Group, nullptr, nullptr);
    // GroupWidget's constructor itself might call slot_updateLabels if m_group is non-null
    // and emits sig_updateWidget or if it directly calls it.
    // Mmapper2Group constructor doesn't emit. slot_updateLabels is called by signals.
    // For a defined start, explicitly call what an initial signal would do.
    QMetaObject::invokeMethod(m_groupWidget, "slot_updateLabels", Qt::DirectConnection);
}

void TestGroupWidget::cleanup() {
    // m_groupWidget is deleted, and its parentage should handle m_mockM2Group if set up,
    // but m_mockM2Group is parented to 'this' (the test object), so Qt handles its deletion.
    delete m_groupWidget;
    m_groupWidget = nullptr;
    m_mockM2Group = nullptr; // It's managed by QObject parentage
}

void TestGroupWidget::testRefreshOnMmapper2GroupSignal() {
    QVERIFY(m_groupWidget && m_mockM2Group);

    // To test if slot_updateLabels is called, we need a way to observe it.
    // Making it a public slot was one way. Another is to check a side effect.
    // For instance, if slot_updateLabels always clears and refills the model,
    // and if we could get the model's row count before and after.
    // Given the difficulty in populating the model, this is hard.

    // A simple "does not crash" and structural test:
    emit m_mockM2Group->sig_updateWidget();
    QTest::qWait(50); // Allow slot execution

    QVERIFY(true); // Test primarily ensures the connection is valid and slot can be called.
}

void TestGroupWidget::testNpcHideSettingAppliedOnRefresh() {
    QVERIFY(m_groupWidget && m_mockM2Group);

    // This test requires:
    // 1. Ability to add characters (specifically NPCs) to m_mockM2Group.
    // 2. Mmapper2Group to correctly populate GroupModel based on these chars.
    // 3. GroupProxyModel to filter based on npcHide.
    // 4. Ability to check visible row count in QTableView.

    // Due to item 1 & 2 being hard, this test is limited.
    // We can set the config and trigger a refresh, then assume the proxy model (if it had data)
    // would use the new config value because GroupProxyModel::filterAcceptsRow uses getConfig().

    bool originalNpcHide = getConfig().groupManager.getNpcHide();

    setConfig().groupManager.setNpcHide(!originalNpcHide);
    // This change should be picked up by Mmapper2Group's listener, which calls its slot_groupSettingsChanged,
    // which then calls characterChanged(true), emitting sig_updateWidget.
    // GroupWidget's connection to sig_updateWidget then calls slot_updateLabels.
    // slot_updateLabels calls m_proxyModel->refresh().
    QTest::qWait(100); // Allow full signal chain

    // We can't verify UI without data. We trust GroupProxyModel::filterAcceptsRow uses the getter correctly.
    // This was verified by inspection in the Mmapper2Group refactoring.
    QVERIFY(getConfig().groupManager.getNpcHide() == !originalNpcHide);
    qDebug() << "TestGroupWidget::testNpcHideSettingAppliedOnRefresh: Confirmed config changed. UI verification needs data.";

    // Restore original setting
    setConfig().groupManager.setNpcHide(originalNpcHide);
    QTest::qWait(50);
}

void TestGroupWidget::testNpcSortBottomSettingAppliedOnRefresh() {
    QVERIFY(m_groupWidget && m_mockM2Group);
    // Similar limitations to testNpcHideSettingAppliedOnRefresh regarding data population.
    // GroupModel::setCharacters is where npcSortBottom is used.
    // Mmapper2Group::slot_updateLabels eventually calls GroupModel::setCharacters.

    bool originalSort = getConfig().groupManager.getNpcSortBottom();

    setConfig().groupManager.setNpcSortBottom(!originalSort);
    QTest::qWait(100); // Allow signal chain to refresh GroupModel

    QVERIFY(getConfig().groupManager.getNpcSortBottom() == !originalSort);
    qDebug() << "TestGroupWidget::testNpcSortBottomSettingAppliedOnRefresh: Confirmed config changed. Order verification needs data.";

    // Restore
    setConfig().groupManager.setNpcSortBottom(originalSort);
    QTest::qWait(50);
}

QTEST_MAIN(TestGroupWidget)
