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

#include <QBuffer>
#include <QDir>
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
            // Only the load we started: one begun from the GUI at the same time must not count.
            const QString requested = QFileInfo{path}.canonicalFilePath();
            std::ignore = waiters.add(
                [requested](const McpEvent &e) {
                    return e.kind == QStringLiteral("map_load")
                           && QFileInfo{e.data.value(QStringLiteral("file")).toString()}
                                      .canonicalFilePath()
                                  == requested;
                },
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

    registry.add(McpTool{
        QStringLiteral("save_map"),
        QStringLiteral(
            "Save an independent copy of the current map and wait until it is written. The map's "
            "own file name, its saved baseline (what `_map diff` compares against) and MMapper's "
            "last-used directory are not changed, so it is safe to call in a test. `format` is "
            "mm2 (the default), xml, mmp or web (web writes a directory). With `base_map` only "
            "the base map is saved. Refuses to replace an existing file or directory unless "
            "`overwrite` is true. Does not need a session."),
        mcpObjectSchema(
            {{QStringLiteral("path"),
              mcpStringProperty(QStringLiteral("Absolute path of the file (or directory, for "
                                               "web) to write."))},
             {QStringLiteral("format"),
              mcpStringProperty(QStringLiteral("mm2 (default), xml, mmp or web."))},
             {QStringLiteral("base_map"),
              QJsonObject{{QStringLiteral("type"), QStringLiteral("boolean")},
                          {QStringLiteral("description"),
                           QStringLiteral("Save only the base map.")}}},
             {QStringLiteral("overwrite"),
              QJsonObject{{QStringLiteral("type"), QStringLiteral("boolean")},
                          {QStringLiteral("description"),
                           QStringLiteral("Replace the file or directory if it exists.")}}},
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
            const QString formatName
                = args.value(QStringLiteral("format")).toString(QStringLiteral("mm2")).toLower();
            SaveFormatEnum format = SaveFormatEnum::MM2;
            if (formatName == QStringLiteral("mm2")) {
                format = SaveFormatEnum::MM2;
            } else if (formatName == QStringLiteral("xml")) {
                format = SaveFormatEnum::MM2XML;
            } else if (formatName == QStringLiteral("mmp")) {
                format = SaveFormatEnum::MMP;
            } else if (formatName == QStringLiteral("web")) {
                format = SaveFormatEnum::WEB;
            } else {
                reply(McpToolResult::error(QStringLiteral("`format` must be mm2, xml, mmp or web")));
                return;
            }
            const bool isWeb = (format == SaveFormatEnum::WEB);
            const SaveModeEnum mode = args.value(QStringLiteral("base_map")).toBool()
                                          ? SaveModeEnum::BASEMAP
                                          : SaveModeEnum::FULL;

            if (info.exists()) {
                if (!args.value(QStringLiteral("overwrite")).toBool()) {
                    reply(McpToolResult::error(
                        QStringLiteral("it exists; pass overwrite=true to replace it: ") + path));
                    return;
                }
                if (info.isDir() != isWeb) {
                    reply(McpToolResult::error(
                        isWeb ? QStringLiteral("web writes a directory, but this is a file: ") + path
                              : QStringLiteral("this is a directory, not a file: ") + path));
                    return;
                }
            }
            const QFileInfo parent{isWeb ? QFileInfo{path}.dir().absolutePath()
                                         : info.absolutePath()};
            if (!parent.isDir() || !parent.isWritable()) {
                reply(McpToolResult::error(
                    QStringLiteral("the directory is missing or not writable: ")
                    + parent.absoluteFilePath()));
                return;
            }
            if (!host.saveMap) {
                reply(McpToolResult::error(QStringLiteral("saving maps is not available")));
                return;
            }

            const int timeoutMs = std::clamp(args.value(QStringLiteral("timeout_ms"))
                                                 .toInt(DEFAULT_LOAD_TIMEOUT_MS),
                                             100,
                                             MCP_MAX_WAIT_MS);
            const QString requested = QDir::cleanPath(info.absoluteFilePath());

            if (const QString error = host.saveMap(path, mode, format); !error.isEmpty()) {
                reply(McpToolResult::error(error));
                return;
            }

            // Only the save we started: one begun from the GUI at the same time must not count.
            std::ignore = waiters.add(
                [requested](const McpEvent &e) {
                    return e.kind == QStringLiteral("map_save")
                           && QDir::cleanPath(
                                  QFileInfo{e.data.value(QStringLiteral("file")).toString()}
                                      .absoluteFilePath())
                                  == requested;
                },
                timeoutMs,
                [path, reply](const McpWaiters::Outcome &outcome) {
                    using Kind = McpWaiters::Outcome::KindEnum;
                    switch (outcome.kind) {
                    case Kind::Matched:
                        if (outcome.event->data.value(QStringLiteral("success")).toBool()) {
                            reply(McpToolResult::json(QJsonObject{{QStringLiteral("path"), path}}));
                        } else {
                            reply(McpToolResult::error(
                                QStringLiteral("the map could not be saved: ") + path));
                        }
                        break;
                    case Kind::TimedOut:
                        reply(McpToolResult::error(
                            QStringLiteral("timed out waiting for the map to be saved; it may "
                                           "still finish (watch for a map_save event)")));
                        break;
                    case Kind::Cancelled:
                        reply(McpToolResult::error(outcome.reason));
                        break;
                    }
                });
        }});

    registry.addSync(
        QStringLiteral("screenshot"),
        QStringLiteral(
            "Render the map canvas as it is shown now and return it as a PNG image "
            "(MMapper's own rendering, not a screen capture). With `path` the PNG is "
            "also written to that absolute path. Right after a load or an edit the canvas may "
            "still show \"the map isn't ready yet\"; take another one a moment later. Fails if "
            "the canvas cannot be rendered. Does not need a session."),
        mcpObjectSchema({{QStringLiteral("path"),
                          mcpStringProperty(QStringLiteral("Absolute path of a .png file to "
                                                           "also write (optional)."))}}),
        [&host](const QJsonObject &args) {
            const QString path = args.value(QStringLiteral("path")).toString();
            if (!path.isEmpty() && !QFileInfo{path}.isAbsolute()) {
                return McpToolResult::error(QStringLiteral("`path` must be an absolute path"));
            }
            if (!host.grabCanvas) {
                return McpToolResult::error(QStringLiteral("screenshots are not available"));
            }
            const QImage image = host.grabCanvas();
            if (image.isNull()) {
                return McpToolResult::error(QStringLiteral("the map canvas could not be rendered"));
            }
            QByteArray png;
            QBuffer buffer{&png};
            if (!buffer.open(QIODevice::WriteOnly) || !image.save(&buffer, "PNG")) {
                return McpToolResult::error(QStringLiteral("could not encode the image"));
            }
            if (!path.isEmpty() && !image.save(path, "PNG")) {
                return McpToolResult::error(QStringLiteral("could not write the file: ") + path);
            }
            return McpToolResult::image(QStringLiteral("%1x%2 PNG, %3 bytes")
                                            .arg(image.width())
                                            .arg(image.height())
                                            .arg(png.size()),
                                        png);
        });
}
