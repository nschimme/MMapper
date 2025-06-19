#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2023 The MMapper Authors

#include "../src/global/macros.h"
#include <QObject>
#include <memory> // For std::unique_ptr

// Forward declarations
class GroupWidget;
class Mmapper2Group;
class Configuration;
// class QListWidget; // GroupWidget uses QTableView
class QTableView;
class GroupModel;
class GroupProxyModel;
// Required for SharedGroupChar if used in helper, even if helper is commented out
#include "../src/group/CGroupChar.h"


class NODISCARD_QOBJECT TestGroupWidget final : public QObject
{
    Q_OBJECT

public:
    TestGroupWidget();
    ~TestGroupWidget() final;

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    void testRefreshOnMmapper2GroupSignal();
    void testNpcHideSettingAppliedOnRefresh();
    void testNpcSortBottomSettingAppliedOnRefresh();
    // void testColorDisplayOnRefresh();

private:
    Mmapper2Group *m_mockM2Group = nullptr;
    GroupWidget *m_groupWidget = nullptr;

    QTableView* getGroupTableView() const;
    GroupModel* getGroupModel() const; // May not be easily implementable
    GroupProxyModel* getGroupProxyModel() const; // May not be easily implementable

    void addTestCharacterToM2G(const QString& name, bool isPlayer, bool isNpc, int idVal = 0); // Implementation deferred
};
