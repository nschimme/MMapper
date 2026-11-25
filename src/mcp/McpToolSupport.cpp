// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "McpToolSupport.h"

#include "../proxy/proxy.h"

#include <memory>
#include <utility>

#include <QMetaObject>
#include <QObject>

QString mcpNoSessionMessage()
{
    return QStringLiteral("there is no session; call open_session first");
}

void mcpWaitWhileSession(McpWaiters &waiters,
                         Proxy *const session,
                         McpWaiters::Predicate predicate,
                         const int timeoutMs,
                         McpWaiters::Done done)
{
    // Shared with the finish callback, which has to drop the connection again.
    auto sessionGone = std::make_shared<QMetaObject::Connection>();
    const uint64_t id = waiters.add(std::move(predicate),
                                    timeoutMs,
                                    [sessionGone,
                                     finish = std::move(done)](const McpWaiters::Outcome &outcome) {
                                        QObject::disconnect(*sessionGone);
                                        finish(outcome);
                                    });
    if (session != nullptr) {
        *sessionGone = QObject::connect(session, &QObject::destroyed, &waiters, [&waiters, id]() {
            std::ignore = waiters.cancel(id, QStringLiteral("the session closed"));
        });
    }
}
