#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2024 The MMapper Authors

#include "../src/global/macros.h"

#include <QObject>

class NODISCARD_QOBJECT TestShortestPath final : public QObject
{
    Q_OBJECT

public:
    TestShortestPath();
    ~TestShortestPath() final;

private Q_SLOTS:
    void shortestPathSearchTest();
};
