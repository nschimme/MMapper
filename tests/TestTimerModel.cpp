// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2024 The MMapper Authors

#include "TestTimerModel.h"

#include "../src/mainwindow/TimerModel.h"
#include "../src/timers/CTimers.h"

#include <QtTest/QtTest>

TestTimerModel::TestTimerModel() = default;
TestTimerModel::~TestTimerModel() = default;

void TestTimerModel::testBasicProperties()
{
    CTimers timers(nullptr);
    TimerModel model(timers);

    QCOMPARE(model.columnCount(), static_cast<int>(TimerModel::ColCount));
    QCOMPARE(model.rowCount(), 0);

    timers.addTimer("T1", "D1");
    QCOMPARE(model.rowCount(), 1);

    timers.addCountdown("C1", "D2", 5000);
    QCOMPARE(model.rowCount(), 2);
}

void TestTimerModel::testDataRetrieval()
{
    CTimers timers(nullptr);
    TimerModel model(timers);

    timers.addCountdown("C1", "Desc1", 10000);

    QModelIndex idxName = model.index(0, TimerModel::ColName);
    QCOMPARE(model.data(idxName).toString(), QString("C1"));

    QModelIndex idxDesc = model.index(0, TimerModel::ColDescription);
    QCOMPARE(model.data(idxDesc).toString(), QString("Desc1"));

    // Progress role
    double progress = model.data(idxName, TimerModel::ProgressRole).toDouble();
    QVERIFY(progress > 0.9 && progress <= 1.0);

    // Stop and check expired color
    timers.stopCountdown("C1");
    QVariant foreground = model.data(idxName, Qt::ForegroundRole);
    QVERIFY(foreground.isValid());
}

void TestTimerModel::testModelUpdates()
{
    CTimers timers(nullptr);
    TimerModel model(timers);
    QSignalSpy spy(&model, &TimerModel::modelReset);

    timers.addTimer("T1", "D1");
    QCOMPARE(spy.count(), 1);

    std::ignore = timers.removeTimer("T1");
    QCOMPARE(spy.count(), 2);
}

QTEST_MAIN(TestTimerModel)
