// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#pragma once

#include <QObject>

class TestPreferences : public QObject
{
    Q_OBJECT

public:
    TestPreferences();
    ~TestPreferences() override;

private slots:
    void initTestCase();
    void configValueTest();
    void configTest();
    void configPersistenceTest();
    void configLegacyColorTest();
    void configValidatorTest();
    void cleanupTestCase() {}
};
