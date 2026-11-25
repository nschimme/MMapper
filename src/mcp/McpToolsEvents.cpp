// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "McpEventLog.h"
#include "McpToolRegistry.h"
#include "McpToolSupport.h"
#include "McpTools.h"
#include "McpWaiters.h"

#include <algorithm>
#include <set>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

namespace {} // namespace

void mcpRegisterEventTools(McpToolRegistry &registry, McpEventLog &events, McpWaiters &waiters)
{
    registry.addSync(
        QStringLiteral("read_events"),
        QStringLiteral(
            "Read what happened since a cursor. Kinds: user_text (everything shown to "
            "the user: MUD text and MMapper's own), prompt (a prompt was shown; fires "
            "even for a repeated one), gmcp (GMCP from the MUD) and sent_to_mud (what "
            "was sent to the MUD), mud_connected / mud_disconnected (the connection to the MUD), position (the current room changed; data.roomId is MMapper's internal id, data.externalRoomId the one get_room takes), "
            "pathmachine (its state), map_changed, map_load and map_save (a map file finished loading or saving; data.file, data.success). Pass the returned nextSeq as `since` next time. "
            "`truncated` is true if older events were dropped before you read them."),
        mcpObjectSchema(
            {{QStringLiteral("since"),
              mcpIntegerProperty(QStringLiteral("Return events after this sequence number "
                                                "(0 for everything still kept)."))},
             {QStringLiteral("kinds"),
              QJsonObject{{QStringLiteral("type"), QStringLiteral("array")},
                          {QStringLiteral("items"),
                           QJsonObject{{QStringLiteral("type"), QStringLiteral("string")}}},
                          {QStringLiteral("description"), QStringLiteral("Only these kinds.")}}},
             {QStringLiteral("max"),
              mcpIntegerProperty(QStringLiteral("At most this many events (default 100, "
                                                "limit 1000)."))}}),
        [&events](const QJsonObject &args) {
            const double since = args.value(QStringLiteral("since")).toDouble(0);
            if (since < 0) {
                return McpToolResult::error(QStringLiteral("`since` must not be negative"));
            }
            const int maxArg = args.value(QStringLiteral("max")).toInt(100);
            const size_t max = static_cast<size_t>(std::clamp(maxArg, 1, 1000));

            std::set<QString> kinds;
            for (const QJsonValue v : args.value(QStringLiteral("kinds")).toArray()) {
                kinds.insert(v.toString());
            }

            const auto read = events.readSince(static_cast<uint64_t>(since), kinds, max);
            QJsonArray list;
            for (const McpEvent &e : read.events) {
                list.append(mcpEventToJson(e));
            }
            QJsonObject result;
            result[QStringLiteral("events")] = list;
            result[QStringLiteral("nextSeq")] = static_cast<double>(read.nextSeq);
            result[QStringLiteral("truncated")] = read.truncated;
            return McpToolResult::json(result);
        });

    registry.add(McpTool{
        QStringLiteral("wait_for_event"),
        QStringLiteral(
            "Wait for the first event that matches, then return it. Events already logged after "
            "`since` count too, so there is no race between acting and waiting: take "
            "`lastEventSeq` from get_status (or nextSeq from read_events) before you act, then "
            "pass it as `since`. All the conditions you give must hold: `kinds` (any of these "
            "kinds), `text_regex` (matches the event's text), and `data` (an object; every key "
            "must equal that key of the event's data, for example {\"success\": true}). Returns "
            "status \"matched\" with the event, or \"timeout\"; either way nextSeq continues "
            "from where this stopped. Does not need a session."),
        mcpObjectSchema(
            {{QStringLiteral("since"),
              mcpIntegerProperty(QStringLiteral("Only events after this sequence number "
                                                "(default: only events from now on)."))},
             {QStringLiteral("kinds"),
              QJsonObject{{QStringLiteral("type"), QStringLiteral("array")},
                          {QStringLiteral("items"),
                           QJsonObject{{QStringLiteral("type"), QStringLiteral("string")}}},
                          {QStringLiteral("description"), QStringLiteral("Only these kinds.")}}},
             {QStringLiteral("text_regex"),
              mcpStringProperty(QStringLiteral("Regular expression the event's text must "
                                               "match."))},
             {QStringLiteral("data"),
              QJsonObject{{QStringLiteral("type"), QStringLiteral("object")},
                          {QStringLiteral("description"),
                           QStringLiteral("Keys that must equal the event's data.")}}},
             {QStringLiteral("timeout_ms"),
              mcpIntegerProperty(QStringLiteral("How long to wait (default 10000, at most "
                                                "100000)."))}}),
        [&events, &waiters](const QJsonObject &args, McpReply reply) {
            const double sinceArg = args.value(QStringLiteral("since")).toDouble(-1);
            const uint64_t since = sinceArg < 0 ? events.lastSeq()
                                                : static_cast<uint64_t>(sinceArg);

            std::set<QString> kinds;
            for (const QJsonValue v : args.value(QStringLiteral("kinds")).toArray()) {
                kinds.insert(v.toString());
            }

            QRegularExpression regex;
            const bool hasRegex = args.contains(QStringLiteral("text_regex"));
            if (hasRegex) {
                regex.setPattern(args.value(QStringLiteral("text_regex")).toString());
                if (!regex.isValid()) {
                    reply(McpToolResult::error(QStringLiteral("`text_regex` is not valid: ")
                                               + regex.errorString()));
                    return;
                }
            }
            const QJsonObject wanted = args.value(QStringLiteral("data")).toObject();

            auto matches = [kinds, hasRegex, regex, wanted](const McpEvent &e) {
                if (!kinds.empty() && kinds.count(e.kind) == 0) {
                    return false;
                }
                if (hasRegex && !regex.match(e.text).hasMatch()) {
                    return false;
                }
                for (auto it = wanted.begin(); it != wanted.end(); ++it) {
                    if (e.data.value(it.key()) != it.value()) {
                        return false;
                    }
                }
                return true;
            };

            auto matchedReply = [](const McpEvent &e) {
                return McpToolResult::json(
                    QJsonObject{{QStringLiteral("status"), QStringLiteral("matched")},
                                {QStringLiteral("event"), mcpEventToJson(e)},
                                {QStringLiteral("nextSeq"), static_cast<double>(e.seq)}});
            };

            // Already happened?
            const auto read = events.readSince(since, kinds);
            for (const McpEvent &e : read.events) {
                if (matches(e)) {
                    reply(matchedReply(e));
                    return;
                }
            }

            const int timeoutMs
                = std::clamp(args.value(QStringLiteral("timeout_ms")).toInt(MCP_DEFAULT_WAIT_MS),
                             100,
                             MCP_MAX_WAIT_MS);
            // Events that arrive between the read above and the waiter are not possible: this
            // all runs on the main thread, and nothing is appended in between.
            std::ignore
                = waiters.add(matches,
                              timeoutMs,
                              [&events, matchedReply, truncated = read.truncated, reply](
                                  const McpWaiters::Outcome &outcome) {
                                  using Kind = McpWaiters::Outcome::KindEnum;
                                  switch (outcome.kind) {
                                  case Kind::Matched:
                                      reply(matchedReply(*outcome.event));
                                      break;
                                  case Kind::TimedOut:
                                      reply(McpToolResult::json(
                                          QJsonObject{{QStringLiteral("status"),
                                                       QStringLiteral("timeout")},
                                                      {QStringLiteral("nextSeq"),
                                                       static_cast<double>(events.lastSeq())},
                                                      {QStringLiteral("truncated"), truncated}}));
                                      break;
                                  case Kind::Cancelled:
                                      reply(McpToolResult::error(outcome.reason));
                                      break;
                                  }
                              });
        }});
}
