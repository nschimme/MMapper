// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "../map/ExitDirection.h"
#include "../map/Map.h"
#include "../map/RoomHandle.h"
#include "../map/mmapper2room.h"
#include "../mapdata/mapdata.h"
#include "McpBackend.h"
#include "McpToolRegistry.h"
#include "McpTools.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace {

NODISCARD QString fromView(const std::string_view sv)
{
    return QString::fromUtf8(sv.data(), static_cast<qsizetype>(sv.size()));
}

NODISCARD QJsonObject roomToJson(const Map &map, const RoomHandle &room)
{
    QJsonObject obj;
    obj[QStringLiteral("id")] = static_cast<double>(room.getIdExternal().asUint32());
    if (const auto server = room.getServerId(); server != INVALID_SERVER_ROOMID) {
        obj[QStringLiteral("serverId")] = static_cast<double>(server.asUint32());
    }
    obj[QStringLiteral("name")] = room.getName().toQString();
    obj[QStringLiteral("area")] = room.getArea().toQString();
    obj[QStringLiteral("description")] = room.getDescription().toQString();
    obj[QStringLiteral("contents")] = room.getContents().toQString();
    obj[QStringLiteral("note")] = room.getNote().toQString();
    obj[QStringLiteral("terrain")] = fromView(to_string_view(room.getTerrainType()));
    obj[QStringLiteral("light")] = fromView(to_string_view(room.getLightType()));
    obj[QStringLiteral("temporary")] = room.isTemporary();

    const Coordinate position = room.getPosition();
    obj[QStringLiteral("position")] = QJsonObject{{QStringLiteral("x"), position.x},
                                                  {QStringLiteral("y"), position.y},
                                                  {QStringLiteral("z"), position.z}};

    // Only the directions that lead somewhere, each with the rooms it leads to.
    QJsonObject exits;
    for (const ExitDirEnum dir : ALL_EXITS_NESWUD) {
        const auto &exit = room.getExit(dir);
        if (!exit.exitIsExit()) {
            continue;
        }
        QJsonArray targets;
        for (const RoomId target : exit.getOutgoingSet()) {
            targets.append(static_cast<double>(map.getExternalRoomId(target).asUint32()));
        }
        QJsonObject e;
        e[QStringLiteral("to")] = targets;
        if (exit.exitIsDoor()) {
            e[QStringLiteral("door")] = exit.getDoorName().toQString();
        }
        exits[QString::fromLatin1(lowercaseDirection(dir))] = e;
    }
    obj[QStringLiteral("exits")] = exits;
    return obj;
}

} // namespace

void mcpRegisterRoomTools(McpToolRegistry &registry, const McpHost &host)
{
    registry.addSync(
        QStringLiteral("get_room"),
        QStringLiteral("Describe a room on the map: name, area, description, terrain, position and "
                       "exits (with the rooms they lead to). Without `id` it describes the room "
                       "the player is in. Room ids are the ones MMapper shows in its dialogs "
                       "and `_room` output."),
        mcpObjectSchema({{QStringLiteral("id"),
                          mcpIntegerProperty(QStringLiteral("The room id. Omit for the current "
                                                            "room."))}}),
        [&host](const QJsonObject &args) {
            const Map map = host.mapData.getCurrentMap();
            RoomHandle room;
            if (args.contains(QStringLiteral("id"))) {
                const double id = args.value(QStringLiteral("id")).toDouble(-1);
                if (id < 0 || id >= static_cast<double>(UINT_MAX)) {
                    return McpToolResult::error(QStringLiteral("`id` is not a valid room id"));
                }
                room = map.findRoomHandle(ExternalRoomId{static_cast<uint32_t>(id)});
                if (!room.exists()) {
                    return McpToolResult::error(QStringLiteral("there is no room with that id"));
                }
            } else {
                const auto current = host.mapData.getCurrentRoomId();
                if (!current) {
                    return McpToolResult::error(
                        QStringLiteral("the player is not in a known room; pass `id`"));
                }
                room = map.findRoomHandle(*current);
                if (!room.exists()) {
                    return McpToolResult::error(
                        QStringLiteral("the player is not in a known room; pass `id`"));
                }
            }
            const QJsonObject json = roomToJson(map, room);
            return McpToolResult::json(json);
        });
}
