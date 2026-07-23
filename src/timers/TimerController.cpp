// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "TimerController.h"

#include "CTimers.h"
#include "TimerModel.h"

#include <tuple>

TimerController::TimerController(CTimers &timers, TimerModel &model, QObject *parent)
    : QObject(parent)
    , m_timers(timers)
    , m_model(model)
{}

void TimerController::reset(int row)
{
    const TTimer *timer = m_model.timerAt(row);
    if (timer == nullptr) {
        return;
    }
    const std::string name = timer->getName();
    if (timer->isCountdown()) {
        m_timers.resetCountdown(name);
    } else {
        m_timers.resetTimer(name);
    }
}

void TimerController::stop(int row)
{
    const TTimer *timer = m_model.timerAt(row);
    if (timer == nullptr) {
        return;
    }
    const std::string name = timer->getName();
    if (timer->isCountdown()) {
        m_timers.stopCountdown(name);
    } else {
        m_timers.stopTimer(name);
    }
}

void TimerController::remove(int row)
{
    const TTimer *timer = m_model.timerAt(row);
    if (timer == nullptr) {
        return;
    }
    const std::string name = timer->getName();
    if (timer->isCountdown()) {
        std::ignore = m_timers.removeCountdown(name);
    } else {
        std::ignore = m_timers.removeTimer(name);
    }
}

void TimerController::clearExpired()
{
    m_timers.clearExpired();
}

void TimerController::move(int from, int to)
{
    m_model.moveRow(from, to);
}
