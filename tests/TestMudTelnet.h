// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2023 The MMapper Authors

#pragma once

#include <QObject>
#include <QtTest/QtTest>

#include "../src/proxy/MudTelnet.h"
#include <QSharedPointer>

class TestMudTelnet : public QObject
{
    Q_OBJECT

private slots:
    void coreHelloTest();
};
