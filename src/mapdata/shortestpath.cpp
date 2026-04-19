// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: 'Elval' <ethorondil@gmail.com> (Elval)

#include "shortestpath.h"

#include "../global/enums.h"
#include "../global/utils.h"
#include "../map/ExitDirection.h"
#include "../map/ExitFlags.h"
#include "../map/RoomIdSet.h"
#include "../map/exit.h"
#include "../map/mmapper2room.h"
#include "../map/room.h"
#include "../map/roomid.h"
#include "mapdata.h"
#include "roomfilter.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

#include <queue>

namespace {
static constexpr const double COST_UNDEFINED = 1.0;
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
static constexpr const double COST_ROAD_BONUS = 0.1;
static constexpr const double COST_DEATHTRAP = 1000.0;

using SPNodeIdx = std::uint32_t;
static constexpr const SPNodeIdx INVALID_SPNODE_IDX = std::numeric_limits<SPNodeIdx>::max();
static constexpr const std::size_t INITIAL_NODES_CAPACITY = 2048;

struct SPNode final
{
    RoomId id;
    SPNodeIdx parent = INVALID_SPNODE_IDX;
    double dist = 0.0;
    ExitDirEnum lastdir = ExitDirEnum::UNKNOWN;
};

NODISCARD static double terrain_cost(const RoomTerrainEnum type)
{
    switch (type) {
#define X_CASE(NAME)           \
    case RoomTerrainEnum::NAME: \
        return COST_##NAME;
        XFOREACH_RoomTerrainEnum(X_CASE)
#undef X_CASE
    }

    return COST_UNDEFINED;
}

NODISCARD static double getLength(const RawExit &e, const RoomHandle &curr, const RoomHandle &nextr)
{
    double cost = terrain_cost(nextr.getTerrainType());
    const auto flags = e.getExitFlags();
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
        cost -= COST_ROAD_BONUS;
    }
    if (nextr.getLoadFlags().contains(RoomLoadFlagEnum::DEATHTRAP)) {
        cost += COST_DEATHTRAP;
    }
    return cost;
}
} // namespace

ShortestPathRecipient::~ShortestPathRecipient() = default;

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
    RoomIdSet visited;
    using DistIdx = std::pair<double, SPNodeIdx>;
    std::priority_queue<DistIdx, std::vector<DistIdx>, std::greater<DistIdx>> future_paths;

    sp_nodes.reserve(INITIAL_NODES_CAPACITY);
    sp_nodes.push_back(SPNode{origin.getId(), INVALID_SPNODE_IDX, 0, ExitDirEnum::UNKNOWN});
    future_paths.emplace(0.0, 0);

    while (!future_paths.empty()) {
        const SPNodeIdx spidx = utils::pop_top(future_paths).second;

        const RoomId room_id = sp_nodes[spidx].id;
        const double thisdist = sp_nodes[spidx].dist;

        if (visited.contains(room_id)) {
            continue;
        }
        visited.insert(room_id);

        const auto &thisr = map.getRoomHandle(room_id);
        if (!thisr) {
            continue;
        }

        if (f.filter(thisr.getRaw())) {
            ShortestPathResult result;
            result.id = room_id;
            result.dist = thisdist;

            SPNodeIdx curr = spidx;
            while (curr != INVALID_SPNODE_IDX && sp_nodes[curr].parent != INVALID_SPNODE_IDX) {
                result.path.push_back(sp_nodes[curr].lastdir);
                curr = sp_nodes[curr].parent;
            }
            std::reverse(result.path.begin(), result.path.end());

            recipient.receiveShortestPath(map, std::move(result));
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
            if (visited.contains(nextrId)) {
                continue;
            }

            const auto &nextr = map.getRoomHandle(nextrId);
            if (!nextr) {
                continue;
            }

            const double length = getLength(e, thisr, nextr);
            const double new_dist = thisdist + length;

            if (max_dist != 0.0 && new_dist > max_dist) {
                continue;
            }

            sp_nodes.push_back(SPNode{nextrId, spidx, new_dist, dir});
            future_paths.emplace(new_dist, static_cast<SPNodeIdx>(sp_nodes.size() - 1));
        }
    }
}
