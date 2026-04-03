#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include <QtTest/QtTest>

class TestMedia : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void musicManagerStopTest();
    void sfxManagerStopTest();
    void audioManagerStopTest();
    void cleanupTestCase();
};
