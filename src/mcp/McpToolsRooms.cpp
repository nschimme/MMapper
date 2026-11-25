// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "../map/DoorFlags.h"
#include "../map/ExitDirection.h"
#include "../map/ExitFlags.h"
#include "../map/Map.h"
#include "../map/RoomHandle.h"
#include "../map/enums.h"
#include "../map/mmapper2room.h"
#include "../mapdata/mapdata.h"
#include "McpBackend.h"
#include "McpToolRegistry.h"
#include "McpToolSupport.h"
#include "McpTools.h"

#include <algorithm>
#include <climits>
#include <map>
#include <vector>

#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

namespace {

NODISCARD QString fromView(const std::string_view sv)
{
    return QString::fromUtf8(sv.data(), static_cast<qsizetype>(sv.size()));
}

template<typename Enum, typename Flags, typename All>
NODISCARD QJsonArray flagNames(const Flags &flags, const All &all)
{
    QJsonArray names;
    for (const Enum flag : all) {
        if (flags.contains(flag)) {
            names.append(fromView(to_string_view(flag)));
        }
    }
    return names;
}

NODISCARD QJsonObject roomToJson(const Map &map, const RoomHandle &room)
{
    QJsonObject obj;
    obj[QStringLiteral("id")] = static_cast<double>(room.getIdExternal().asUint32());
    obj[QStringLiteral("internalId")] = static_cast<double>(room.getId().asUint32());
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
    obj[QStringLiteral("align")] = fromView(to_string_view(room.getAlignType()));
    obj[QStringLiteral("portable")] = fromView(to_string_view(room.getPortableType()));
    obj[QStringLiteral("ridable")] = fromView(to_string_view(room.getRidableType()));
    obj[QStringLiteral("sundeath")] = fromView(to_string_view(room.getSundeathType()));
    obj[QStringLiteral("mobFlags")] = flagNames<RoomMobFlagEnum>(room.getMobFlags(), ALL_MOB_FLAGS);
    obj[QStringLiteral("loadFlags")] = flagNames<RoomLoadFlagEnum>(room.getLoadFlags(),
                                                                   ALL_LOAD_FLAGS);
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
        // Sorted by external id: the set iterates in internal-id order, which is not stable.
        std::vector<uint32_t> ids;
        for (const RoomId target : exit.getOutgoingSet()) {
            ids.push_back(map.getExternalRoomId(target).asUint32());
        }
        std::sort(ids.begin(), ids.end());
        QJsonArray targets;
        for (const uint32_t id : ids) {
            targets.append(static_cast<double>(id));
        }
        QJsonObject e;
        e[QStringLiteral("to")] = targets;
        const auto &fields = exit.getExitFields();
        e[QStringLiteral("exitFlags")] = flagNames<ExitFlagEnum>(fields.exitFlags, ALL_EXIT_FLAGS);
        if (exit.exitIsDoor()) {
            e[QStringLiteral("doorFlags")] = flagNames<DoorFlagEnum>(fields.doorFlags,
                                                                     ALL_DOOR_FLAGS);
        }
        if (exit.exitIsDoor()) {
            e[QStringLiteral("door")] = exit.getDoorName().toQString();
        }
        exits[QString::fromLatin1(lowercaseDirection(dir))] = e;
    }
    obj[QStringLiteral("exits")] = exits;
    return obj;
}

NODISCARD QJsonObject boundsToJson(const Bounds &b)
{
    auto point = [](const Coordinate &c) {
        return QJsonObject{{QStringLiteral("x"), c.x},
                           {QStringLiteral("y"), c.y},
                           {QStringLiteral("z"), c.z}};
    };
    return QJsonObject{{QStringLiteral("min"), point(b.min)}, {QStringLiteral("max"), point(b.max)}};
}

// Every room, in external-id order, so output is stable and can be diffed.
NODISCARD std::vector<RoomHandle> sortedRooms(const Map &map)
{
    std::vector<RoomHandle> rooms;
    rooms.reserve(map.getRoomsCount());
    map.getRooms().for_each([&map, &rooms](const RoomId id) {
        if (const RoomHandle room = map.findRoomHandle(id); room.exists()) {
            rooms.push_back(room);
        }
    });
    std::sort(rooms.begin(), rooms.end(), [](const RoomHandle &a, const RoomHandle &b) {
        return a.getIdExternal().asUint32() < b.getIdExternal().asUint32();
    });
    return rooms;
}

NODISCARD QJsonObject briefRoom(const RoomHandle &room)
{
    const Coordinate p = room.getPosition();
    return QJsonObject{{QStringLiteral("id"), static_cast<double>(room.getIdExternal().asUint32())},
                       {QStringLiteral("name"), room.getName().toQString()},
                       {QStringLiteral("area"), room.getArea().toQString()},
                       {QStringLiteral("x"), p.x},
                       {QStringLiteral("y"), p.y},
                       {QStringLiteral("z"), p.z}};
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

void mcpRegisterMapInspectionTools(McpToolRegistry &registry, const McpHost &host)
{
    registry.addSync(QStringLiteral("map_summary"),
                     QStringLiteral(
                         "Summarize the current map: room and marker counts, bounds, rooms per area, "
                         "the current room, the map file name and whether it has unsaved changes. "
                         "Does not need a session."),
                     mcpObjectSchema(),
                     [&host](const QJsonObject &) {
                         const Map map = host.mapData.getCurrentMap();
                         std::map<QString, int> areas;
                         for (const RoomHandle &room : sortedRooms(map)) {
                             ++areas[room.getArea().toQString()];
                         }
                         QJsonObject areaJson;
                         for (const auto &[name, count] : areas) {
                             areaJson[name] = count;
                         }

                         QJsonObject result;
                         result[QStringLiteral("rooms")] = static_cast<double>(map.getRoomsCount());
                         result[QStringLiteral("marks")] = static_cast<double>(map.getMarksCount());
                         result[QStringLiteral("bounds")] = map.getBounds()
                                                                ? QJsonValue{boundsToJson(
                                                                      *map.getBounds())}
                                                                : QJsonValue{QJsonValue::Null};
                         result[QStringLiteral("areas")] = areaJson;
                         if (const auto id = host.mapData.getCurrentRoomId()) {
                             result[QStringLiteral("currentRoomId")] = static_cast<double>(
                                 map.getExternalRoomId(*id).asUint32());
                         } else {
                             result[QStringLiteral("currentRoomId")] = QJsonValue::Null;
                         }
                         if (const auto id = host.mapData.getCurrentRoomId()) {
                             result[QStringLiteral("currentRoomInternalId")] = static_cast<double>(
                                 id->asUint32());
                         }
                         result[QStringLiteral("fileName")] = host.mapData.getFileName();
                         result[QStringLiteral("modified")] = host.mapData.dataChanged();
                         return McpToolResult::json(result);
                     });

    registry.addSync(
        QStringLiteral("list_rooms"),
        QStringLiteral("List rooms (id, name, area, position) in ascending id order, optionally "
                       "filtered by exact `area`, a case-insensitive `name` substring, and/or a "
                       "bounding box (`min_x`..`max_z`, any subset). Use `offset` and `limit` to "
                       "page; `total` is the number of matches. Does not need a session."),
        mcpObjectSchema(
            {{QStringLiteral("area"), mcpStringProperty(QStringLiteral("Exact area name."))},
             {QStringLiteral("name"),
              mcpStringProperty(QStringLiteral("Case-insensitive substring of the room name."))},
             {QStringLiteral("min_x"), mcpIntegerProperty(QStringLiteral("Smallest x."))},
             {QStringLiteral("max_x"), mcpIntegerProperty(QStringLiteral("Largest x."))},
             {QStringLiteral("min_y"), mcpIntegerProperty(QStringLiteral("Smallest y."))},
             {QStringLiteral("max_y"), mcpIntegerProperty(QStringLiteral("Largest y."))},
             {QStringLiteral("min_z"), mcpIntegerProperty(QStringLiteral("Smallest z."))},
             {QStringLiteral("max_z"), mcpIntegerProperty(QStringLiteral("Largest z."))},
             {QStringLiteral("offset"),
              mcpIntegerProperty(QStringLiteral("Matches to skip (default 0)."))},
             {QStringLiteral("limit"),
              mcpIntegerProperty(QStringLiteral("At most this many (default 100, limit 1000)."))}}),
        [&host](const QJsonObject &args) {
            auto bound = [&args](const char *key, const int fallback) {
                return args.contains(QString::fromLatin1(key))
                           ? args.value(QString::fromLatin1(key)).toInt(fallback)
                           : fallback;
            };
            const int minX = bound("min_x", INT_MIN), maxX = bound("max_x", INT_MAX);
            const int minY = bound("min_y", INT_MIN), maxY = bound("max_y", INT_MAX);
            const int minZ = bound("min_z", INT_MIN), maxZ = bound("max_z", INT_MAX);
            const bool hasArea = args.contains(QStringLiteral("area"));
            const QString area = args.value(QStringLiteral("area")).toString();
            const QString name = args.value(QStringLiteral("name")).toString();
            const int offset = std::max(0, args.value(QStringLiteral("offset")).toInt(0));
            const int limit = std::clamp(args.value(QStringLiteral("limit")).toInt(100), 1, 1000);

            const Map map = host.mapData.getCurrentMap();
            QJsonArray list;
            int total = 0;
            for (const RoomHandle &room : sortedRooms(map)) {
                const Coordinate p = room.getPosition();
                if (p.x < minX || p.x > maxX || p.y < minY || p.y > maxY || p.z < minZ
                    || p.z > maxZ) {
                    continue;
                }
                if (hasArea && room.getArea().toQString() != area) {
                    continue;
                }
                if (!name.isEmpty()
                    && !room.getName().toQString().contains(name, Qt::CaseInsensitive)) {
                    continue;
                }
                if (total >= offset && list.size() < limit) {
                    list.append(briefRoom(room));
                }
                ++total;
            }
            QJsonObject result;
            result[QStringLiteral("rooms")] = list;
            result[QStringLiteral("total")] = total;
            result[QStringLiteral("offset")] = offset;
            return McpToolResult::json(result);
        });

    registry.addSync(
        QStringLiteral("dump_map"),
        QStringLiteral(
            "Write the current map's rooms as JSON to a file: every room (as get_room "
            "describes it, including flags) in ascending id order, with sorted keys, so "
            "two dumps can be compared with diff. Infomarks are not included. Refuses to replace an existing file unless "
            "`overwrite` is true. Does not need a session."),
        mcpObjectSchema({{QStringLiteral("path"),
                          mcpStringProperty(QStringLiteral("Absolute path of the file to write."))},
                         {QStringLiteral("overwrite"),
                          QJsonObject{{QStringLiteral("type"), QStringLiteral("boolean")},
                                      {QStringLiteral("description"),
                                       QStringLiteral("Replace the file if it exists.")}}}},
                        {QStringLiteral("path")}),
        [&host](const QJsonObject &args) {
            const QString path = args.value(QStringLiteral("path")).toString();
            const QFileInfo info{path};
            if (path.isEmpty() || !info.isAbsolute()) {
                return McpToolResult::error(QStringLiteral("`path` must be an absolute path"));
            }
            if (info.exists() && !args.value(QStringLiteral("overwrite")).toBool()) {
                return McpToolResult::error(
                    QStringLiteral("the file exists; pass overwrite=true to replace it: ") + path);
            }

            const Map map = host.mapData.getCurrentMap();
            QJsonArray rooms;
            for (const RoomHandle &room : sortedRooms(map)) {
                rooms.append(roomToJson(map, room));
            }
            QJsonObject doc;
            doc[QStringLiteral("rooms")] = rooms;
            doc[QStringLiteral("roomCount")] = rooms.size();

            QSaveFile file{path};
            if (!file.open(QIODevice::WriteOnly)
                || file.write(QJsonDocument{doc}.toJson(QJsonDocument::Indented)) < 0
                || !file.commit()) {
                return McpToolResult::error(QStringLiteral("cannot write the file: ") + path);
            }
            QJsonObject result;
            result[QStringLiteral("path")] = path;
            result[QStringLiteral("rooms")] = rooms.size();
            return McpToolResult::json(result);
        });
}
