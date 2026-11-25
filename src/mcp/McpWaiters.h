#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "../global/macros.h"
#include "McpEventLog.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

#include <QObject>
#include <QString>
#include <QTimer>

// Things that are waiting for an event: "tell me when the prompt arrives", with a time limit.
//
// This is what lets a tool answer later without a nested event loop (which would re-enter the
// parser and proxy): the tool registers a waiter and returns, and the reply goes out from
// onEvent(), the timeout, or cancel(). Every waiter finishes exactly once.
class NODISCARD McpWaiters final : public QObject
{
public:
    struct NODISCARD Outcome final
    {
        enum class KindEnum { Matched, TimedOut, Cancelled };
        KindEnum kind = KindEnum::Matched;
        QString reason;                // why it was cancelled
        std::optional<McpEvent> event; // the event that matched
    };

    using Predicate = std::function<bool(const McpEvent &)>;
    using Done = std::function<void(const Outcome &)>;

private:
    struct Waiter final
    {
        uint64_t id = 0;
        Predicate predicate;
        Done done;
        QTimer timer;
    };
    std::vector<std::unique_ptr<Waiter>> m_waiters;
    std::vector<std::unique_ptr<Waiter>> m_finished; // freed once control is back in the event loop
    uint64_t m_nextId = 1;

public:
    explicit McpWaiters(QObject *parent = nullptr);
    ~McpWaiters() final;

    // `done` is called exactly once. The id can be passed to cancel().
    NODISCARD uint64_t add(Predicate predicate, int timeoutMs, Done done);

    // Offers a new event to every waiter; the ones it satisfies finish.
    void onEvent(const McpEvent &event);

    // Finishes one waiter as cancelled. Returns false if it had already finished.
    ALLOW_DISCARD bool cancel(uint64_t id, const QString &reason);
    void cancelAll(const QString &reason);

    NODISCARD size_t pending() const { return m_waiters.size(); }

private:
    // Removes the waiter and calls its done callback after it is no longer in the list, so
    // done may add or cancel other waiters.
    bool finish(uint64_t id, const Outcome &outcome);
};
