// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "McpEventLog.h"
#include "McpToolRegistry.h"
#include "McpTools.h"

#include <algorithm>
#include <set>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace {

NODISCARD QJsonObject eventToJson(const McpEvent &e)
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

} // namespace

void mcpRegisterEventTools(McpToolRegistry &registry, McpEventLog &events)
{
    registry.addSync(
        QStringLiteral("read_events"),
        QStringLiteral(
            "Read what happened since a cursor. Kinds: user_text (everything shown to "
            "the user: MUD text and MMapper's own), prompt (a prompt was shown; fires "
            "even for a repeated one), gmcp (GMCP from the MUD) and sent_to_mud (what "
            "was sent to the MUD), position (the current room changed; data.roomId), "
            "pathmachine (its state), and map_changed. Pass the returned nextSeq as `since` next time. "
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
            for (const QJsonValue &v : args.value(QStringLiteral("kinds")).toArray()) {
                kinds.insert(v.toString());
            }

            const auto read = events.readSince(static_cast<uint64_t>(since), kinds, max);
            QJsonArray list;
            for (const McpEvent &e : read.events) {
                list.append(eventToJson(e));
            }
            QJsonObject result;
            result[QStringLiteral("events")] = list;
            result[QStringLiteral("nextSeq")] = static_cast<double>(read.nextSeq);
            result[QStringLiteral("truncated")] = read.truncated;
            return McpToolResult::ok(QString::fromUtf8(
                                         QJsonDocument{result}.toJson(QJsonDocument::Compact)),
                                     result);
        });
}
