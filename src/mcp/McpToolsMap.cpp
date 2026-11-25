// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "../map/Map.h"
#include "../mapdata/mapdata.h"
#include "McpBackend.h"
#include "McpToolRegistry.h"
#include "McpToolSupport.h"
#include "McpTools.h"
#include "McpWaiters.h"

#include <algorithm>

#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>

namespace {

constexpr int DEFAULT_LOAD_TIMEOUT_MS = 60'000;

NODISCARD McpToolResult loadedResult(const McpHost &host, const QString &file)
{
    QJsonObject result;
    result[QStringLiteral("file")] = file;
    result[QStringLiteral("rooms")] = static_cast<double>(
        host.mapData.getCurrentMap().getRoomsCount());
    return McpToolResult::json(result);
}

} // namespace

void mcpRegisterMapTools(McpToolRegistry &registry, const McpHost &host, McpWaiters &waiters)
{
    registry.add(McpTool{
        QStringLiteral("load_map"),
        QStringLiteral("Replace the current map with a map file (for example a test fixture) and "
                       "wait until it is loaded. The current map is discarded first. Fails if "
                       "the file cannot be read or another file operation is running."),
        mcpObjectSchema({{QStringLiteral("path"),
                          mcpStringProperty(QStringLiteral("Absolute path of the map file."))},
                         {QStringLiteral("timeout_ms"),
                          mcpIntegerProperty(QStringLiteral("How long to wait (default 60000, at most "
                                                            "100000)."))}},
                        {QStringLiteral("path")}),
        [&host, &waiters](const QJsonObject &args, McpReply reply) {
            const QString path = args.value(QStringLiteral("path")).toString();
            const QFileInfo info{path};
            if (path.isEmpty() || !info.isAbsolute()) {
                reply(McpToolResult::error(QStringLiteral("`path` must be an absolute path")));
                return;
            }
            if (!info.isFile() || !info.isReadable()) {
                reply(McpToolResult::error(QStringLiteral("cannot read the file: ") + path));
                return;
            }
            if (!host.loadMap) {
                reply(McpToolResult::error(QStringLiteral("loading maps is not available")));
                return;
            }

            const int timeoutMs = std::clamp(args.value(QStringLiteral("timeout_ms"))
                                                 .toInt(DEFAULT_LOAD_TIMEOUT_MS),
                                             100,
                                             MCP_MAX_WAIT_MS);

            if (const QString error = host.loadMap(path); !error.isEmpty()) {
                reply(McpToolResult::error(error));
                return;
            }

            // The load runs in the background; its end shows up in the event log.
            std::ignore = waiters.add(
                [](const McpEvent &e) { return e.kind == QStringLiteral("map_load"); },
                timeoutMs,
                [&host, path, reply](const McpWaiters::Outcome &outcome) {
                    using Kind = McpWaiters::Outcome::KindEnum;
                    switch (outcome.kind) {
                    case Kind::Matched:
                        if (outcome.event->data.value(QStringLiteral("success")).toBool()) {
                            reply(loadedResult(host, path));
                        } else {
                            reply(McpToolResult::error(
                                QStringLiteral("the map could not be loaded: ") + path));
                        }
                        break;
                    case Kind::TimedOut:
                        reply(McpToolResult::error(
                            QStringLiteral("timed out waiting for the map to load; it may still "
                                           "finish (watch for a map_load event)")));
                        break;
                    case Kind::Cancelled:
                        reply(McpToolResult::error(outcome.reason));
                        break;
                    }
                });
        }});
}
