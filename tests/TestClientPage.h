#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2023 The MMapper Authors

#include "../src/global/macros.h"
#include <QObject>
#include <memory> // For std::unique_ptr

// Forward declarations
class ClientPage;
class Configuration;
class QFontComboBox;
class QPushButton;
class QSpinBox;
class QCheckBox;

class NODISCARD_QOBJECT TestClientPage final : public QObject
{
    Q_OBJECT

public:
    TestClientPage();
    ~TestClientPage() final;

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    void testLoadSettings();
    void testChangeFontSetting();
    void testChangeColorSetting(); // e.g., foregroundColor
    void testChangeIntSetting();   // e.g., columns
    void testChangeBoolSetting();  // e.g., clearInputOnEnter
    void testSignalEmissionOnConfigChange(); // Tests signal from ClientPage
    void testUiUpdatesOnExternalConfigChange(); // Tests if UI updates if config changed elsewhere

private:
    ClientPage *m_clientPage = nullptr;
    std::unique_ptr<Configuration> m_pristineDefaultConfig;
};
