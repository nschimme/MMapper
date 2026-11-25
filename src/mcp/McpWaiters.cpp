// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "McpWaiters.h"

#include <algorithm>
#include <utility>

McpWaiters::McpWaiters(QObject *const parent)
    : QObject{parent}
{}

McpWaiters::~McpWaiters() = default;

uint64_t McpWaiters::add(Predicate predicate, const int timeoutMs, Done done)
{
    auto waiter = std::make_unique<Waiter>();
    waiter->id = m_nextId++;
    waiter->predicate = std::move(predicate);
    waiter->done = std::move(done);
    waiter->timer.setSingleShot(true);

    const uint64_t id = waiter->id;
    connect(&waiter->timer, &QTimer::timeout, this, [this, id]() {
        Outcome outcome;
        outcome.kind = Outcome::KindEnum::TimedOut;
        finish(id, outcome);
    });
    waiter->timer.start(std::max(0, timeoutMs));
    m_waiters.push_back(std::move(waiter));
    return id;
}

void McpWaiters::onEvent(const McpEvent &event)
{
    // Work from a list of ids: finishing a waiter runs arbitrary code that may change m_waiters.
    std::vector<uint64_t> matched;
    for (const auto &waiter : m_waiters) {
        if (waiter->predicate(event)) {
            matched.push_back(waiter->id);
        }
    }
    for (const uint64_t id : matched) {
        Outcome outcome;
        outcome.kind = Outcome::KindEnum::Matched;
        outcome.event = event;
        finish(id, outcome);
    }
}

bool McpWaiters::cancel(const uint64_t id, const QString &reason)
{
    Outcome outcome;
    outcome.kind = Outcome::KindEnum::Cancelled;
    outcome.reason = reason;
    return finish(id, outcome);
}

void McpWaiters::cancelAll(const QString &reason)
{
    std::vector<uint64_t> ids;
    for (const auto &waiter : m_waiters) {
        ids.push_back(waiter->id);
    }
    for (const uint64_t id : ids) {
        std::ignore = cancel(id, reason);
    }
}

bool McpWaiters::finish(const uint64_t id, const Outcome &outcome)
{
    const auto it = std::find_if(m_waiters.begin(), m_waiters.end(), [id](const auto &w) {
        return w->id == id;
    });
    if (it == m_waiters.end()) {
        return false;
    }
    std::unique_ptr<Waiter> waiter = std::move(*it);
    m_waiters.erase(it);
    waiter->timer.stop();

    // The timer is a member of the waiter, and finish() may be running inside its timeout
    // signal, so the waiter is only destroyed after control returns to the event loop.
    Done done = std::move(waiter->done);
    m_finished.push_back(std::move(waiter));
    QTimer::singleShot(0, this, [this]() { m_finished.clear(); });
    if (done) {
        done(outcome);
    }
    return true;
}
