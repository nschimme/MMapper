#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include <QObject>

class TestUserActionManager : public QObject
{
    Q_OBJECT

public:
    TestUserActionManager();
    ~TestUserActionManager() override;

private slots:
    void testBasicAction();
    void testRegexCaptures();
    void testStartsWithEndsWith();
    void testSerialization();
    void testActionRemoval();
};
