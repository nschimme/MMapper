#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#include "../src/global/macros.h"

#include <QObject>

class NODISCARD_QOBJECT TestPreferences final : public QObject
{
    Q_OBJECT

public:
    TestPreferences();
    ~TestPreferences() final;

private Q_SLOTS:
    void initTestCase();
    void configValueTest();
    void configTest();
    void configDialogTest();
};
