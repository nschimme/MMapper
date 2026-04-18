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
#include <utility>
#include <vector>

#include <queue>

namespace {
static constexpr const double COST_DEFAULT = 1.0;
static constexpr const double COST_INDOORS = 0.75;
static constexpr const double COST_CITY = 0.75;
static constexpr const double COST_FIELD = 1.5;
static constexpr const double COST_FOREST = 2.15;
static constexpr const double COST_HILLS = 2.45;
static constexpr const double COST_MOUNTAINS = 2.8;
static constexpr const double COST_SHALLOW = 2.45;
static constexpr const double COST_WATER = 50.0;
static constexpr const double COST_RAPIDS = 60.0;
static constexpr const double COST_UNDERWATER = 100.0;
static constexpr const double COST_ROAD = 0.85;
static constexpr const double COST_BRUSH = 1.5;
static constexpr const double COST_TUNNEL = 0.75;
static constexpr const double COST_CAVERN = 0.75;

static constexpr const double COST_RANDOM_DAMAGE_FALL = 30.0;
static constexpr const double COST_DOOR = 1.0;
static constexpr const double COST_CLIMB = 2.0;
static constexpr const double COST_NOT_RIDABLE = 3.0;
static constexpr const double COST_DISMOUNT = 4.0;
static constexpr const double COST_ROAD_ADJUSTMENT = 0.1;
static constexpr const double COST_DEATHTRAP = 1000.0;

static constexpr const std::size_t INITIAL_NODES_CAPACITY = 1024;
} // namespace

ShortestPathRecipient::~ShortestPathRecipient() = default;

NODISCARD static double terrain_cost(const RoomTerrainEnum type)
{
    switch (type) {
    case RoomTerrainEnum::UNDEFINED:
        return COST_DEFAULT;
    case RoomTerrainEnum::INDOORS:
        return COST_INDOORS;
    case RoomTerrainEnum::CITY:
        return COST_CITY;
    case RoomTerrainEnum::FIELD:
        return COST_FIELD;
    case RoomTerrainEnum::FOREST:
        return COST_FOREST;
    case RoomTerrainEnum::HILLS:
        return COST_HILLS;
    case RoomTerrainEnum::MOUNTAINS:
        return COST_MOUNTAINS;
    case RoomTerrainEnum::SHALLOW:
        return COST_SHALLOW;
    case RoomTerrainEnum::WATER:
        return COST_WATER;
    case RoomTerrainEnum::RAPIDS:
        return COST_RAPIDS;
    case RoomTerrainEnum::UNDERWATER:
        return COST_UNDERWATER;
    case RoomTerrainEnum::ROAD:
        return COST_ROAD;
    case RoomTerrainEnum::BRUSH:
        return COST_BRUSH;
    case RoomTerrainEnum::TUNNEL:
        return COST_TUNNEL;
    case RoomTerrainEnum::CAVERN:
        return COST_CAVERN;
    }

    return COST_DEFAULT;
}

NODISCARD static double getLength(const RawExit &e, const RawRoom &curr, const RawRoom &nextr)
{
    double cost = terrain_cost(nextr.getTerrainType());
    auto flags = e.getExitFlags();
    if (flags.isRandom() || flags.isDamage() || flags.isFall()) {
        cost += COST_RANDOM_DAMAGE_FALL;
    }
    if (flags.isDoor()) {
        cost += COST_DOOR;
    }
    if (flags.isClimb()) {
        cost += COST_CLIMB;
    }
    if (nextr.getRidableType() == RoomRidableEnum::NOT_RIDABLE) {
        cost += COST_NOT_RIDABLE;
        // One non-ridable room means walking two rooms, plus dismount/mount.
        if (curr.getRidableType() != RoomRidableEnum::NOT_RIDABLE) {
            cost += COST_DISMOUNT;
        }
    }
    if (flags.isRoad()) {
        cost -= COST_ROAD_ADJUSTMENT;
    }
    if (nextr.getLoadFlags().contains(RoomLoadFlagEnum::DEATHTRAP)) {
        cost += COST_DEATHTRAP;
    }
    return cost;
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

    std::vector<SPNode> sp_nodes;
    std::vector<uint8_t> visited(map.getRoomsCount() + 1, 0);
    using DistIdx = std::pair<double, int>;
    std::priority_queue<DistIdx, std::vector<DistIdx>, std::greater<DistIdx>> future_paths;

    sp_nodes.reserve(INITIAL_NODES_CAPACITY);
    sp_nodes.push_back(SPNode{origin.getId(), -1, 0, ExitDirEnum::UNKNOWN});
    future_paths.emplace(0.0, 0);

    while (!future_paths.empty()) {
        const int spindex = future_paths.top().second;
        future_paths.pop();

        const std::size_t spidx = static_cast<std::size_t>(spindex);
        const RoomId room_id = sp_nodes[spidx].id;
        const double thisdist = sp_nodes[spidx].dist;

        if (room_id.asUint32() < visited.size()) {
            if (visited[room_id.asUint32()]) {
                continue;
            }
            visited[room_id.asUint32()] = 1;
        }

        const auto &thisr = map.getRoomHandle(room_id);
        if (!thisr) {
            continue;
        }

        // Omit starting room on purpose
        if (spindex != 0 && f.filter(thisr.getRaw())) {
            recipient.receiveShortestPath(map, sp_nodes, spindex);
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
                continue;
            }
            if (!e.exitIsExit()) {
                continue;
            }

            const RoomId nextrId = e.getOutgoingSet().first();
            if (nextrId.asUint32() < visited.size() && visited[nextrId.asUint32()]) {
                continue;
            }

            const auto &nextr = map.getRoomHandle(nextrId);
            if (!nextr) {
                continue;
            }

            const double length = getLength(e, thisr.getRaw(), nextr.getRaw());
            const double new_dist = thisdist + length;

            if (max_dist != 0.0 && new_dist > max_dist) {
                continue;
            }

            sp_nodes.push_back(SPNode{nextrId, spindex, new_dist, dir});
            future_paths.emplace(new_dist, static_cast<int>(sp_nodes.size() - 1));
        }
    }
}
