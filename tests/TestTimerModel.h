#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2024 The MMapper Authors

#include "../src/global/macros.h"

#include <QObject>

class NODISCARD_QOBJECT TestTimerModel final : public QObject
{
    Q_OBJECT

public:
    TestTimerModel();
    ~TestTimerModel() final;

private Q_SLOTS:
    void testBasicProperties();
    void testDataRetrieval();
    void testModelUpdates();
};
