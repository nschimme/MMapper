#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "../global/macros.h"

#include <QObject>

class CTimers;
class TimerModel;

// Lifts the operations offered by TimerWidget's context menu into a
// QObject that can be driven from either C++ or (in a later step) QML.
class NODISCARD_QOBJECT TimerController final : public QObject
{
    Q_OBJECT

private:
    CTimers &m_timers;
    TimerModel &m_model;

public:
    explicit TimerController(CTimers &timers, TimerModel &model, QObject *parent = nullptr);

public:
    Q_INVOKABLE void reset(int row);
    Q_INVOKABLE void stop(int row);
    Q_INVOKABLE void remove(int row);
    Q_INVOKABLE void clearExpired();
};
