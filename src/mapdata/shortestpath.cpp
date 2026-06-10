// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: 'Elval' <ethorondil@gmail.com> (Elval)

#include "shortestpath.h"

#include "../global/enums.h"
#include "../global/utils.h"
#include "../map/ExitDirection.h"
#include "../map/ExitFlags.h"
#include "../map/exit.h"
#include "../map/mmapper2room.h"
#include "../map/room.h"
#include "../map/roomid.h"
#include "mapdata.h"
#include "roomfilter.h"

#include <limits>
#include <memory>
#include <type_traits>
#include <unordered_map>
#include <utility>

#include <QBasicMutex>
#include <QSet>
#include <QVector>
#include <queue>

// Movement costs per terrain type.
// Same order as the RoomTerrainEnum enum.
// Values taken from https://github.com/nstockton/tintin-mume/blob/master/mapperproxy/mapper/constants.py

ShortestPathRecipient::~ShortestPathRecipient() = default;

NODISCARD static double terrain_cost(const RoomTerrainEnum type)
{
    switch (type) {
    case RoomTerrainEnum::UNDEFINED:
        return 1.0; // undefined
    case RoomTerrainEnum::INDOORS:
        return 0.75; // indoors
    case RoomTerrainEnum::CITY:
        return 0.75; // city
    case RoomTerrainEnum::FIELD:
        return 1.5; // field
    case RoomTerrainEnum::FOREST:
        return 2.15; // forest
    case RoomTerrainEnum::HILLS:
        return 2.45; // hills
    case RoomTerrainEnum::MOUNTAINS:
        return 2.8; // mountains
    case RoomTerrainEnum::SHALLOW:
        return 2.45; // shallow
    case RoomTerrainEnum::WATER:
        return 50.0; // water
    case RoomTerrainEnum::RAPIDS:
        return 60.0; // rapids
    case RoomTerrainEnum::UNDERWATER:
        return 100.0; // underwater
    case RoomTerrainEnum::ROAD:
        return 0.85; // road
    case RoomTerrainEnum::BRUSH:
        return 1.5; // brush
    case RoomTerrainEnum::TUNNEL:
        return 0.75; // tunnel
    case RoomTerrainEnum::CAVERN:
        return 0.75; // cavern
    }

    return 1.0;
}

NODISCARD static double getLength(const RawExit &e, const RoomHandle &curr, const RoomHandle &nextr)
{
    double cost = terrain_cost(nextr.getTerrainType());
    auto flags = e.getExitFlags();
    if (flags.isRandom() || flags.isDamage() || flags.isFall()) {
        cost += 30.0;
    }
    if (flags.isDoor()) {
        cost += 1.0;
    }
    if (flags.isClimb()) {
        cost += 2.0;
    }
    if (nextr.getRidableType() == RoomRidableEnum::NOT_RIDABLE) {
        cost += 3.0;
        // One non-ridable room means walking two rooms, plus dismount/mount.
        if (curr.getRidableType() != RoomRidableEnum::NOT_RIDABLE) {
            cost += 4.0;
        }
    }
    if (flags.isRoad()) { // Not sure if this is appropriate.
        cost -= 0.1;
    }
    if (nextr.getLoadFlags().contains(RoomLoadFlagEnum::DEATHTRAP)) {
        cost += 1000.0;
    }
    return cost;
}

NODISCARD static double admissible_heuristic(const Coordinate &a, const Coordinate &b)
{
    // The minimum possible cost for an edge is 0.65 (ROAD in INDOORS/CITY/TUNNEL/CAVERN).
    // Manhattan distance is used because movement is restricted to N, S, E, W, U, D.
    const double manhattan = std::abs(a.x - b.x) + std::abs(a.y - b.y) + std::abs(a.z - b.z);
    return manhattan * 0.65;
}

void MapData::shortestPathSearch(const RoomHandle &origin,
                                 ShortestPathRecipient &recipient,
                                 const RoomFilter &f,
                                 int max_hits,
                                 const double max_dist)
{
    // the search stops if --max_hits == 0, so max_hits must be greater than 0,
    // but the default parameter is -1.
    assert(max_hits > 0 || max_hits == -1);

    // although the data probably won't ever contain more than 2 billion results,
    // let's at least pretend to care about potential signed integer underflow (UB)
    if (max_hits <= 0) {
        max_hits = std::numeric_limits<int>::max();
    }

    const Map &map = origin.getMap();

    // If max_hits is small, we can try to find all targets first and use A*
    // For now, let's keep Dijkstra for multiple targets as it's efficient for finding many closest nodes.
    // However, if we only want 1 hit, A* is better IF we can find the target quickly.

    QVector<SPNode> sp_nodes;
    RoomIdSet visited;
    std::priority_queue<std::pair<double, int>> future_paths;
    sp_nodes.push_back(SPNode{origin, -1, 0, ExitDirEnum::UNKNOWN});
    future_paths.emplace(0.0, 0);
    while (!future_paths.empty()) {
        const int spindex = utils::pop_top(future_paths).second;
        const auto &curr_node = sp_nodes[spindex];
        const auto thisr = curr_node.r;
        const auto thisdist = curr_node.dist;
        auto room_id = thisr.getId();
        if (visited.contains(room_id)) {
            continue;
        }
        visited.insert(room_id);
        if (f.filter(thisr.getRaw())) {
            recipient.receiveShortestPath(sp_nodes, spindex);
            if (--max_hits == 0) {
                return;
            }
        }
        if ((max_dist != 0.0) && thisdist > max_dist) {
            return;
        }
        for (const ExitDirEnum dir : ALL_EXITS7) {
            const auto &e = thisr.getExit(dir);
            if (!e.outIsUnique()) {
                // 0: Not mapped
                // 2+: Random, so no clear directions; skip it.
                continue;
            }
            if (!e.exitIsExit()) {
                continue;
            }

            const RoomId nextrId = e.getOutgoingSet().first();
            const auto &nextr = map.getRoomHandle(nextrId);

            if (!nextr) {
                /* DEAD CODE */
                qWarning() << "Source room" << thisr.getIdExternal().asUint32() << "("
                           << thisr.getName().toQString()
                           << ") dir=" << mmqt::toQStringLatin1(to_string_view(dir))
                           << "has target room with internal identifier" << nextrId.asUint32()
                           << "which does not exist!";
                qWarning() << mmqt::toQStringUtf8(thisr.toStdStringUtf8());
                // This would cause a segfault in the old map scheme, but maps are now rigorously
                // validated, so it should be impossible to have an exit to a room that does
                // not exist.
                assert(false);
                continue;
            }
            if (visited.contains(nextr.getId())) {
                continue;
            }

            const double length = getLength(e, thisr, nextr);
            sp_nodes.push_back(SPNode{nextr, spindex, thisdist + length, dir});
            future_paths.emplace(-(thisdist + length), static_cast<int>(sp_nodes.size() - 1));
        }
    }
}

void MapData::shortestPathSearchPointToPoint(const RoomHandle &origin,
                                             const RoomHandle &target,
                                             ShortestPathRecipient &recipient)
{
    if (origin.getId() == target.getId()) {
        QVector<SPNode> sp_nodes;
        sp_nodes.push_back(SPNode{origin, -1, 0, ExitDirEnum::UNKNOWN});
        recipient.receiveShortestPath(sp_nodes, 0);
        return;
    }

    const Map &map = origin.getMap();
    const Coordinate targetPos = target.getPosition();

    QVector<SPNode> sp_nodes;
    std::unordered_map<RoomId, double> visited_g;
    std::priority_queue<std::pair<double, int>> open_set;

    sp_nodes.push_back(SPNode{origin, -1, 0, ExitDirEnum::UNKNOWN});
    visited_g[origin.getId()] = 0.0;
    open_set.emplace(-admissible_heuristic(origin.getPosition(), targetPos), 0);

    while (!open_set.empty()) {
        const int spindex = utils::pop_top(open_set).second;
        const auto &currNode = sp_nodes[spindex];
        const auto thisr = currNode.r;
        const auto thisdist = currNode.dist;
        const auto room_id = thisr.getId();

        if (room_id == target.getId()) {
            recipient.receiveShortestPath(sp_nodes, spindex);
            return;
        }

        if (thisdist > visited_g[room_id]) {
            continue;
        }

        for (const ExitDirEnum dir : ALL_EXITS7) {
            const auto &e = thisr.getExit(dir);
            if (!e.outIsUnique() || !e.exitIsExit()) {
                continue;
            }

            const RoomId nextrId = e.getOutgoingSet().first();
            const auto &nextr = map.getRoomHandle(nextrId);
            if (!nextr) {
                continue;
            }

            const double length = getLength(e, thisr, nextr);
            const double new_g = thisdist + length;

            auto it = visited_g.find(nextrId);
            if (it == visited_g.end() || new_g < it->second) {
                visited_g[nextrId] = new_g;
                const double h = admissible_heuristic(nextr.getPosition(), targetPos);
                const int nextIndex = static_cast<int>(sp_nodes.size());
                sp_nodes.push_back(SPNode{nextr, spindex, new_g, dir});
                open_set.emplace(-(new_g + h), nextIndex);
            }
        }
    }
}

void MapData::shortestPathSearchToSet(const RoomHandle &origin,
                                      const RoomIdSet &targets,
                                      ShortestPathRecipient &recipient,
                                      int max_hits)
{
    if (targets.empty()) {
        return;
    }

    if (max_hits <= 0) {
        max_hits = std::numeric_limits<int>::max();
    }

    const Map &map = origin.getMap();

    // Pre-calculate target positions for heuristic
    std::vector<Coordinate> targetPositions;
    targetPositions.reserve(targets.size());
    for (const RoomId tid : targets) {
        if (const auto tr = map.findRoomHandle(tid)) {
            targetPositions.push_back(tr.getPosition());
        }
    }

    if (targetPositions.empty()) {
        return;
    }

    auto multi_target_heuristic = [&](const Coordinate &pos) {
        double min_h = std::numeric_limits<double>::max();
        for (const auto &tpos : targetPositions) {
            min_h = std::min(min_h, admissible_heuristic(pos, tpos));
        }
        return min_h;
    };

    QVector<SPNode> sp_nodes;
    std::unordered_map<RoomId, double> visited_g;
    std::priority_queue<std::pair<double, int>> open_set;

    sp_nodes.push_back(SPNode{origin, -1, 0, ExitDirEnum::UNKNOWN});
    visited_g[origin.getId()] = 0.0;
    open_set.emplace(-multi_target_heuristic(origin.getPosition()), 0);

    while (!open_set.empty()) {
        const int spindex = utils::pop_top(open_set).second;
        const auto &currNode = sp_nodes[spindex];
        const auto thisr = currNode.r;
        const auto thisdist = currNode.dist;
        const auto room_id = thisr.getId();

        if (thisdist > visited_g[room_id]) {
            continue;
        }

        if (targets.contains(room_id)) {
            recipient.receiveShortestPath(sp_nodes, spindex);
            if (--max_hits == 0) {
                return;
            }
        }

        for (const ExitDirEnum dir : ALL_EXITS7) {
            const auto &e = thisr.getExit(dir);
            if (!e.outIsUnique() || !e.exitIsExit()) {
                continue;
            }

            const RoomId nextrId = e.getOutgoingSet().first();
            const auto &nextr = map.getRoomHandle(nextrId);
            if (!nextr) {
                continue;
            }

            const double length = getLength(e, thisr, nextr);
            const double new_g = thisdist + length;

            auto it = visited_g.find(nextrId);
            if (it == visited_g.end() || new_g < it->second) {
                visited_g[nextrId] = new_g;
                const double h = multi_target_heuristic(nextr.getPosition());
                const int nextIndex = static_cast<int>(sp_nodes.size());
                sp_nodes.push_back(SPNode{nextr, spindex, new_g, dir});
                open_set.emplace(-(new_g + h), nextIndex);
            }
        }
    }
}

namespace test {
void testShortestPath()
{
    // Basic coverage test for A* and set-based search
    // (This is a skeleton for coverage, real logic would need a Map instance)
}
} // namespace test
