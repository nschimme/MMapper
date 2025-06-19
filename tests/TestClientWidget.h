#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2023 The MMapper Authors

#include "../src/global/macros.h"
#include <QObject>
#include <memory> // For std::unique_ptr

// Forward declarations
class ClientWidget;
class Configuration;
class DisplayWidget; // To access for verification
class InputWidget;   // To access for verification

class NODISCARD_QOBJECT TestClientWidget final : public QObject
{
    Q_OBJECT

public:
    TestClientWidget();
    ~TestClientWidget() final;

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    void testLiveUpdateFont();
    void testLiveUpdateInputHistorySize();
    // Add more tests for other settings like colors, columns, rows if feasible to verify

private:
    ClientWidget *m_clientWidget = nullptr;
    DisplayWidget *m_displayWidget = nullptr; // Assuming we can get a pointer from ClientWidget
    InputWidget *m_inputWidget = nullptr;   // Assuming we can get a pointer from ClientWidget
    // std::unique_ptr<Configuration> m_pristineDefaultConfig; // If needed for defaults
};
