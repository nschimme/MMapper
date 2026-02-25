// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#pragma once

#include <QtTest/QtTest>

class TestFrameManager : public QObject
{
    Q_OBJECT

public:
    TestFrameManager() = default;
    ~TestFrameManager() override = default;

private Q_SLOTS:
    void testTargetFps();
    void testThrottle();
    void testDecoupling();
    void testHammering();
};
