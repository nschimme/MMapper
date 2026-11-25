// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "../proxy/ProxyParserApi.h"
#include "../proxy/connectionlistener.h"
#include "../proxy/proxy.h"
#include "McpBackend.h"
#include "McpEventLog.h"
#include "McpToolRegistry.h"
#include "McpTools.h"
#include "McpWaiters.h"

#include <algorithm>
#include <memory>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaObject>
#include <QPointer>
#include <QRegularExpression>

namespace {

constexpr int DEFAULT_TIMEOUT_MS = 10'000;
constexpr int MAX_TIMEOUT_MS = 100'000; // below the server's own limit on a deferred reply
constexpr size_t MAX_EVENTS_IN_REPLY = 1000;

NODISCARD QJsonObject eventJson(const McpEvent &e)
{
    QJsonObject obj;
    obj[QStringLiteral("seq")] = static_cast<double>(e.seq);
    obj[QStringLiteral("kind")] = e.kind;
    obj[QStringLiteral("text")] = e.text;
    if (!e.data.isEmpty()) {
        obj[QStringLiteral("data")] = e.data;
    }
    return obj;
}

NODISCARD QString compactJson(const QJsonObject &obj)
{
    return QString::fromUtf8(QJsonDocument{obj}.toJson(QJsonDocument::Compact));
}

// The answer to send: everything that happened after `since`, and how the wait ended.
NODISCARD McpToolResult makeReply(const McpEventLog &events,
                                  const uint64_t since,
                                  const QString &waited,
                                  const QString &problem)
{
    const auto read = events.readSince(since, {}, MAX_EVENTS_IN_REPLY);
    QJsonArray list;
    for (const McpEvent &e : read.events) {
        list.append(eventJson(e));
    }
    QJsonObject result;
    result[QStringLiteral("waited")] = waited;
    result[QStringLiteral("events")] = list;
    result[QStringLiteral("nextSeq")] = static_cast<double>(read.nextSeq);
    result[QStringLiteral("truncated")] = read.truncated;

    if (problem.isEmpty()) {
        return McpToolResult::ok(compactJson(result), result);
    }
    // The events so far are still useful when a wait fails, so they travel with the error.
    result[QStringLiteral("error")] = problem;
    return McpToolResult{true, problem + QStringLiteral("\n") + compactJson(result), result};
}

} // namespace

void mcpRegisterDriveTools(McpToolRegistry &registry,
                           const McpHost &host,
                           McpEventLog &events,
                           McpWaiters &waiters)
{
    registry.add(McpTool{
        QStringLiteral("send"),
        QStringLiteral("Send one line as if the user typed it and pressed Enter: a game command, "
                       "or an MMapper command starting with the command prefix (usually '_'). "
                       "Needs a session (see open_session). Returns the events that happened "
                       "because of it. Pass wait_for to wait until something happens: \"prompt\" "
                       "for the next prompt, anything else is a regular expression matched "
                       "against new MUD text and MMapper text. On a timeout the result is an "
                       "error that still contains the events so far."),
        mcpObjectSchema({{QStringLiteral("line"),
                          mcpStringProperty(QStringLiteral("The line to send, without a newline."))},
                         {QStringLiteral("wait_for"),
                          mcpStringProperty(
                              QStringLiteral("\"prompt\", or a regular expression. Omit to "
                                             "return at once."))},
                         {QStringLiteral("timeout_ms"),
                          mcpIntegerProperty(QStringLiteral("How long to wait (default 10000, at most "
                                                            "100000)."))}},
                        {QStringLiteral("line")}),
        [&host, &events, &waiters](const QJsonObject &args, McpReply reply) {
            const QString line = args.value(QStringLiteral("line")).toString();
            if (line.contains(QLatin1Char('\n')) || line.contains(QLatin1Char('\r'))) {
                reply(McpToolResult::error(
                    QStringLiteral("`line` must be a single line; call send once per line")));
                return;
            }
            Proxy *const proxy = host.listener.getProxy();
            if (proxy == nullptr) {
                reply(McpToolResult::error(
                    QStringLiteral("there is no session; call open_session first")));
                return;
            }

            const QString waitFor = args.value(QStringLiteral("wait_for")).toString();
            const int timeoutMs
                = std::clamp(args.value(QStringLiteral("timeout_ms")).toInt(DEFAULT_TIMEOUT_MS),
                             100,
                             MAX_TIMEOUT_MS);

            McpWaiters::Predicate predicate;
            if (waitFor == QStringLiteral("prompt")) {
                predicate = [](const McpEvent &e) { return e.kind == QStringLiteral("prompt"); };
            } else if (!waitFor.isEmpty()) {
                const QRegularExpression re{waitFor};
                if (!re.isValid()) {
                    reply(McpToolResult::error(QStringLiteral("invalid regular expression: ")
                                               + re.errorString()));
                    return;
                }
                predicate = [re](const McpEvent &e) {
                    return (e.kind == QStringLiteral("mud_text")
                            || e.kind == QStringLiteral("user_text"))
                           && re.match(e.text).hasMatch();
                };
            }

            // Everything from here on counts as caused by this line.
            const uint64_t since = events.lastSeq();
            ProxyControlApi{*proxy}.sendUserInput(line.toUtf8());

            if (!predicate) {
                reply(makeReply(events, since, QStringLiteral("none"), {}));
                return;
            }

            // The parser may already have produced what we are waiting for.
            const auto already = events.readSince(since);
            if (std::any_of(already.events.begin(), already.events.end(), predicate)) {
                reply(makeReply(events, since, QStringLiteral("matched"), {}));
                return;
            }

            // Wait. If the session goes away first, there is nothing left to wait for.
            auto sessionGone = std::make_shared<QMetaObject::Connection>();
            const uint64_t id = waiters.add(
                predicate,
                timeoutMs,
                [&events, since, reply, sessionGone](const McpWaiters::Outcome &outcome) {
                    QObject::disconnect(*sessionGone);
                    switch (outcome.kind) {
                    case McpWaiters::Outcome::KindEnum::Matched:
                        reply(makeReply(events, since, QStringLiteral("matched"), {}));
                        break;
                    case McpWaiters::Outcome::KindEnum::TimedOut:
                        reply(makeReply(events,
                                        since,
                                        QStringLiteral("timeout"),
                                        QStringLiteral("timed out waiting for the condition")));
                        break;
                    case McpWaiters::Outcome::KindEnum::Cancelled:
                        reply(makeReply(events, since, QStringLiteral("cancelled"), outcome.reason));
                        break;
                    }
                });
            *sessionGone = QObject::connect(proxy, &QObject::destroyed, &waiters, [&waiters, id]() {
                std::ignore = waiters.cancel(id, QStringLiteral("the session closed"));
            });
        }});
}
