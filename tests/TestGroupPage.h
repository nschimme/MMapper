#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2023 The MMapper Authors

#include "../src/global/macros.h"
#include <QObject>
#include <memory> // For std::unique_ptr

// Forward declarations
class GroupPage;
class Configuration;
class QCheckBox; // Example UI element
class QPushButton; // Example UI element for color

class NODISCARD_QOBJECT TestGroupPage final : public QObject
{
    Q_OBJECT

public:
    TestGroupPage();
    ~TestGroupPage() final;

private slots:
    void initTestCase();    // Called before the first test function is executed
    void cleanupTestCase(); // Called after the last test function was executed
    void init();            // Called before each test function is executed
    void cleanup();         // Called after each test function is executed

    void testLoadSettings();
    void testChangeBoolSetting(); // e.g., npcHide
    void testChangeColorSetting(); // e.g., group color
    void testSignalEmissionOnConfigChange();

private:
    GroupPage *m_groupPage = nullptr;
    // To verify default values if needed, similar to TestConfigDialog
    std::unique_ptr<Configuration> m_pristineDefaultConfig;
};
