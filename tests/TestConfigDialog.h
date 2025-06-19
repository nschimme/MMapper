#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2023 The MMapper Authors

#include "../src/global/macros.h"
#include <QObject>

class ConfigDialog;
class DeveloperPage;
class QListWidget;
class QLineEdit;

class NODISCARD_QOBJECT TestConfigDialog final : public QObject
{
    Q_OBJECT

public:
    TestConfigDialog();
    ~TestConfigDialog() final;

private slots:
    void initTestCase();
    void cleanupTestCase();
    void testDeveloperPageExists();
    void testDeveloperPagePopulation();
    void testDeveloperPageSearch();
    void testDeveloperPageSettingChange();

private:
    ConfigDialog *m_configDialog = nullptr;
    DeveloperPage *m_developerPage = nullptr;
    QListWidget *m_pageListWidget = nullptr;
    QLineEdit *m_searchLineEdit = nullptr;
};
