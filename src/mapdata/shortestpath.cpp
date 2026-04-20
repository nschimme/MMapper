// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: 'Elval' <ethorondil@gmail.com> (Elval)

#include "shortestpath.h"

#include "../global/Timer.h"
#include "../global/thread_utils.h"
#include "../global/utils.h"
#include "../map/ExitDirection.h"
#include "../map/ExitFlags.h"
#include "../map/Map.h"
#include "../map/RoomIdSet.h"
#include "../map/mmapper2room.h"
#include "../map/roomid.h"
#include "GenericFind.h"
#include "mapdata.h"
#include "roomfilter.h"

#include <algorithm>
#include <atomic>
#include <limits>
#include <memory>
#include <mutex>
#include <utility>
#include <vector>

namespace {
// Values taken from https://github.com/nstockton/tintin-mume/blob/master/mapperproxy/mapper/constants.py
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

static constexpr const double DELTA = 2.0;
static constexpr const size_t SHARDS = 1024;
static constexpr const size_t PARALLEL_THRESHOLD = 256;

NODISCARD static double terrain_cost(const RoomTerrainEnum type)
{
    switch (type) {
#define X_CASE(NAME) \
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

struct Shard
{
    alignas(64) std::mutex mutex;
};

bool relax(const RoomId v_id,
           const double v_new_dist,
           const RoomId u_id,
           const ExitDirEnum dir,
           std::atomic<double> *const dists,
           RoomId *const parents,
           ExitDirEnum *const lastdirs,
           Shard *const locks,
           const double max_dist)
{
    if (max_dist != 0.0 && v_new_dist > max_dist) {
        return false;
    }
    const uint32_t v_uint = v_id.asUint32();
    if (v_new_dist < dists[v_uint].load(std::memory_order_relaxed)) {
        std::lock_guard<std::mutex> lock(locks[v_uint % SHARDS].mutex);
        if (v_new_dist < dists[v_uint].load(std::memory_order_relaxed)) {
            dists[v_uint].store(v_new_dist, std::memory_order_relaxed);
            parents[v_uint] = u_id;
            lastdirs[v_uint] = dir;
            return true;
        }
    }
    return false;
}

} // namespace

ShortestPathRecipient::~ShortestPathRecipient() = default;

void MapData::shortestPathSearch(const RoomHandle &origin,
                                 ShortestPathRecipient &recipient,
                                 const RoomFilter &f,
                                 int max_hits,
                                 const double max_dist)
{
    DECL_TIMER(t, "shortestPathSearch");

    // the search stops if --max_hits == 0, so max_hits must be greater than 0,
    // but the default parameter is -1.
    assert(max_hits > 0 || max_hits == -1);

    // although the data probably won't ever contain more than 2 billion results,
    // let's at least pretend to care about potential signed integer underflow (UB)
    if (max_hits <= 0) {
        max_hits = std::numeric_limits<int>::max();
    }

    const Map &map = origin.getMap();
    const RoomIdSet targets = ::genericFind(map, f);
    if (targets.empty()) {
        return;
    }

    // Use sorted property of ImmRoomIdSet to find max room ID.
    const RoomId max_room_id = map.getRooms().last();
    const size_t vec_size = static_cast<size_t>(max_room_id.asUint32()) + 1;

    auto dists = std::make_unique<std::atomic<double>[]>(vec_size);
    auto parents = std::make_unique<RoomId[]>(vec_size);
    auto lastdirs = std::make_unique<ExitDirEnum[]>(vec_size);
    auto locks = std::make_unique<Shard[]>(SHARDS);

    for (size_t i = 0; i < vec_size; ++i) {
        dists[i].store(std::numeric_limits<double>::infinity(), std::memory_order_relaxed);
        parents[i] = INVALID_ROOMID;
        lastdirs[i] = ExitDirEnum::UNKNOWN;
    }

    std::vector<uint8_t> is_target(vec_size, 0);
    for (const RoomId id : targets) {
        is_target[id.asUint32()] = 1;
    }

    std::vector<std::vector<RoomId>> buckets;
    auto get_bucket_idx = [](double d) -> size_t { return static_cast<size_t>(d / DELTA); };

    const RoomId origin_id = origin.getId();
    dists[origin_id.asUint32()].store(0.0, std::memory_order_relaxed);
    size_t start_bucket = get_bucket_idx(0.0);
    buckets.resize(start_bucket + 1);
    buckets[start_bucket].push_back(origin_id);

    size_t current_bucket_idx = start_bucket;
    int total_hits = 0;
    const size_t numThreads = thread_utils::idealThreadCount();

    while (current_bucket_idx < buckets.size() && total_hits < max_hits) {
        if (buckets[current_bucket_idx].empty()) {
            current_bucket_idx++;
            continue;
        }

        std::vector<RoomId> bucket_nodes;
        while (!buckets[current_bucket_idx].empty()) {
            std::vector<RoomId> current_nodes = std::move(buckets[current_bucket_idx]);
            buckets[current_bucket_idx].clear();

            struct TlData
            {
                std::vector<RoomId> light;
            };
            std::vector<RoomId> all_improved_light;

            auto relax_light = [&](std::vector<RoomId> &improved_light, const RoomId u_id) {
                const double u_dist = dists[u_id.asUint32()].load(std::memory_order_relaxed);
                if (get_bucket_idx(u_dist) != current_bucket_idx) {
                    return;
                }
                const auto &u_handle = map.getRoomHandle(u_id);
                for (const ExitDirEnum dir : ALL_EXITS7) {
                    const auto &e = u_handle.getExit(dir);
                    if (!e.outIsUnique() || !e.exitIsExit()) {
                        continue;
                    }
                    const RoomId v_id = e.getOutgoingSet().first();
                    const auto &v_handle = map.getRoomHandle(v_id);
                    const double weight = getLength(e, u_handle, v_handle);
                    if (weight > DELTA) {
                        continue;
                    }
                    if (relax(v_id,
                              u_dist + weight,
                              u_id,
                              dir,
                              dists.get(),
                              parents.get(),
                              lastdirs.get(),
                              locks.get(),
                              max_dist)) {
                        improved_light.push_back(v_id);
                    }
                }
            };

            if (numThreads > 1 && current_nodes.size() > PARALLEL_THRESHOLD) {
                ProgressCounter pc;
                thread_utils::parallel_for_each_tl<TlData>(
                    current_nodes,
                    pc,
                    [&](TlData &tl, const RoomId u_id) { relax_light(tl.light, u_id); },
                    [&](auto &tls) {
                        for (auto &tl : tls) {
                            all_improved_light.insert(
                                all_improved_light.end(), tl.light.begin(), tl.light.end());
                        }
                    });
            } else {
                for (const RoomId u_id : current_nodes) {
                    relax_light(all_improved_light, u_id);
                }
            }

            for (const RoomId v_id : all_improved_light) {
                size_t b = get_bucket_idx(dists[v_id.asUint32()].load());
                if (buckets.size() <= b) {
                    buckets.resize(b + 1);
                }
                buckets[b].push_back(v_id);
            }
            bucket_nodes.insert(bucket_nodes.end(), current_nodes.begin(), current_nodes.end());
        }

        std::sort(bucket_nodes.begin(), bucket_nodes.end());
        bucket_nodes.erase(std::unique(bucket_nodes.begin(), bucket_nodes.end()),
                            bucket_nodes.end());

        struct TlDataHeavy
        {
            std::vector<RoomId> heavy;
        };
        std::vector<RoomId> all_improved_heavy;

        auto relax_heavy = [&](std::vector<RoomId> &improved_heavy, const RoomId u_id) {
            const double u_dist = dists[u_id.asUint32()].load(std::memory_order_relaxed);
            if (get_bucket_idx(u_dist) != current_bucket_idx) {
                return;
            }
            const auto &u_handle = map.getRoomHandle(u_id);
            for (const ExitDirEnum dir : ALL_EXITS7) {
                const auto &e = u_handle.getExit(dir);
                if (!e.outIsUnique() || !e.exitIsExit()) {
                    continue;
                }
                const RoomId v_id = e.getOutgoingSet().first();
                const auto &v_handle = map.getRoomHandle(v_id);
                const double weight = getLength(e, u_handle, v_handle);
                if (weight <= DELTA) {
                    continue;
                }
                if (relax(v_id,
                          u_dist + weight,
                          u_id,
                          dir,
                          dists.get(),
                          parents.get(),
                          lastdirs.get(),
                          locks.get(),
                          max_dist)) {
                    improved_heavy.push_back(v_id);
                }
            }
        };

        if (numThreads > 1 && bucket_nodes.size() > PARALLEL_THRESHOLD) {
            ProgressCounter pc_heavy;
            thread_utils::parallel_for_each_tl<TlDataHeavy>(
                bucket_nodes,
                pc_heavy,
                [&](TlDataHeavy &tl, const RoomId u_id) { relax_heavy(tl.heavy, u_id); },
                [&](auto &tls) {
                    for (auto &tl : tls) {
                        all_improved_heavy.insert(
                            all_improved_heavy.end(), tl.heavy.begin(), tl.heavy.end());
                    }
                });
        } else {
            for (const RoomId u_id : bucket_nodes) {
                relax_heavy(all_improved_heavy, u_id);
            }
        }

        for (const RoomId v_id : all_improved_heavy) {
            size_t b = get_bucket_idx(dists[v_id.asUint32()].load());
            if (buckets.size() <= b) {
                buckets.resize(b + 1);
            }
            buckets[b].push_back(v_id);
        }

        std::vector<RoomId> targets_in_bucket;
        for (const RoomId id : bucket_nodes) {
            if (is_target[id.asUint32()]
                && get_bucket_idx(dists[id.asUint32()].load()) == current_bucket_idx) {
                targets_in_bucket.push_back(id);
            }
        }

        if (!targets_in_bucket.empty()) {
            std::sort(targets_in_bucket.begin(),
                      targets_in_bucket.end(),
                      [&dists](RoomId a, RoomId b) {
                          return dists[a.asUint32()].load() < dists[b.asUint32()].load();
                      });
            for (const RoomId target_id : targets_in_bucket) {
                ShortestPathResult result;
                result.id = target_id;
                result.dist = dists[target_id.asUint32()].load();
                RoomId curr = target_id;
                while (curr != origin_id) {
                    result.path.push_back(lastdirs[curr.asUint32()]);
                    curr = parents[curr.asUint32()];
                    if (curr == INVALID_ROOMID) {
                        break;
                    }
                }
                if (curr == origin_id) {
                    std::reverse(result.path.begin(), result.path.end());
                    recipient.receiveShortestPath(map, std::move(result));
                    if (++total_hits >= max_hits) {
                        return;
                    }
                }
            }
        }
        current_bucket_idx++;
    }
}
