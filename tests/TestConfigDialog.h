#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2023 The MMapper Authors

#include "../src/global/macros.h"
#include <QObject>
#include <QSignalSpy> // Added for signal testing
#include <memory> // For std::unique_ptr

// Forward declare Configuration
class Configuration;

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
        void testDeveloperPageGraphicsSignal();
        void testDeveloperPageResetToDefault();

private:
    ConfigDialog *m_configDialog = nullptr;
    DeveloperPage *m_developerPage = nullptr;
    QListWidget *m_pageListWidget = nullptr;
    QLineEdit *m_searchLineEdit = nullptr;
        std::unique_ptr<Configuration> m_pristineDefaultConfig; // To know actual default values
};
