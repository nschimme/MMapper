#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2023 The MMapper Authors

#include "../src/global/macros.h"

#include <QObject>
#include <QtTest/QtTest>

class NODISCARD_QOBJECT TestGroup final : public QObject
{
    Q_OBJECT

public:
    TestGroup();
    ~TestGroup() final;

private Q_SLOTS:
    void initTestCase();    // Common setup for all tests in this class
    void cleanupTestCase(); // Common cleanup

    void testGroupModelNpcFiltering_data(); // Data for the filtering test
    void testGroupModelNpcFiltering();
    void testPlayerColorPreference();

private:
    // Add any common member variables needed by tests here
};
