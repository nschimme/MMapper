// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: 'Elval' <ethorondil@gmail.com> (Elval)

#include "shortestpath.h"

#include "../global/IndexedVector.h"
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
static constexpr const float COST_UNDEFINED = 1.0f;
static constexpr const float COST_INDOORS = 0.75f;
static constexpr const float COST_CITY = 0.75f;
static constexpr const float COST_FIELD = 1.5f;
static constexpr const float COST_FOREST = 2.15f;
static constexpr const float COST_HILLS = 2.45f;
static constexpr const float COST_MOUNTAINS = 2.8f;
static constexpr const float COST_SHALLOW = 2.45f;
static constexpr const float COST_WATER = 50.0f;
static constexpr const float COST_RAPIDS = 60.0f;
static constexpr const float COST_UNDERWATER = 100.0f;
static constexpr const float COST_ROAD = 0.85f;
static constexpr const float COST_BRUSH = 1.5f;
static constexpr const float COST_TUNNEL = 0.75f;
static constexpr const float COST_CAVERN = 0.75f;

static constexpr const float COST_RANDOM_DAMAGE_FALL = 30.0f;
static constexpr const float COST_DOOR = 1.0f;
static constexpr const float COST_CLIMB = 2.0f;
static constexpr const float COST_NOT_RIDABLE = 3.0f;
static constexpr const float COST_DISMOUNT = 4.0f;
static constexpr const float COST_ROAD_BONUS = 0.1f;
static constexpr const float COST_DEATHTRAP = 1000.0f;

static constexpr const float DELTA = 2.0f;
static constexpr const size_t MAX_SHARDS = 1024;

NODISCARD static float terrain_cost(const RoomTerrainEnum type)
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

NODISCARD static float getLength(const RawExit &e, const RoomHandle &curr, const RoomHandle &nextr)
{
    float cost = terrain_cost(nextr.getTerrainType());
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
           const float v_new_dist,
           const RoomId u_id,
           const ExitDirEnum dir,
           std::atomic<float> *const dists,
           IndexedVector<RoomId, RoomId> &parents,
           IndexedVector<ExitDirEnum, RoomId> &lastdirs,
           Shard *const locks,
           const size_t numShards,
           const float max_dist)
{
    if (max_dist != 0.0f && v_new_dist > max_dist) {
        return false;
    }
    if (v_new_dist < dists[v_id.asUint32()].load(std::memory_order_relaxed)) {
        std::lock_guard<std::mutex> lock(locks[v_id.asUint32() % numShards].mutex);
        if (v_new_dist < dists[v_id.asUint32()].load(std::memory_order_relaxed)) {
            dists[v_id.asUint32()].store(v_new_dist, std::memory_order_relaxed);
            parents.at(v_id) = u_id;
            lastdirs.at(v_id) = dir;
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
                                 const double max_dist_d)
{
    DECL_TIMER(t, "shortestPathSearch");

    const float max_dist = static_cast<float>(max_dist_d);

    // the search stops if --max_hits == 0, so max_hits must be greater than 0,
    // but the default parameter is -1.
    assert(max_hits > 0 || max_hits == -1);

    if (max_hits <= 0) {
        max_hits = std::numeric_limits<int>::max();
    }

    const Map &map = origin.getMap();
    const RoomIdSet targets = ::genericFind(map, f);
    if (targets.empty()) {
        return;
    }

    const RoomId max_room_id = map.getRooms().last();
    const size_t vec_size = static_cast<size_t>(max_room_id.asUint32()) + 1;

    auto dists = std::make_unique<std::atomic<float>[]>(vec_size);
    IndexedVector<RoomId, RoomId> parents;
    IndexedVector<ExitDirEnum, RoomId> lastdirs;

    parents.resize(vec_size);
    lastdirs.resize(vec_size);

    const size_t numThreads = thread_utils::idealThreadCount();
    const size_t numShards = numThreads > 1
                                 ? std::min<size_t>(MAX_SHARDS,
                                                    utils::nextPowerOfTwo(numThreads * 16))
                                 : 1;
    auto locks = std::make_unique<Shard[]>(numShards);

    for (size_t i = 0; i < vec_size; ++i) {
        dists[i].store(std::numeric_limits<float>::infinity(), std::memory_order_relaxed);
    }
    std::fill(parents.begin(), parents.end(), INVALID_ROOMID);
    std::fill(lastdirs.begin(), lastdirs.end(), ExitDirEnum::UNKNOWN);

    std::vector<std::vector<RoomId>> buckets;
    auto get_bucket_idx = [](float d) -> size_t { return static_cast<size_t>(d / DELTA); };

    const RoomId origin_id = origin.getId();
    dists[origin_id.asUint32()].store(0.0f, std::memory_order_relaxed);
    size_t start_bucket = get_bucket_idx(0.0f);
    buckets.resize(start_bucket + 1);
    buckets[start_bucket].push_back(origin_id);

    size_t current_bucket_idx = start_bucket;
    int total_hits = 0;

    std::atomic<float> *const pDists = dists.get();
    Shard *const pLocks = locks.get();

    auto relax_node = [&map,
                       pDists,
                       &parents,
                       &lastdirs,
                       pLocks,
                       numShards,
                       max_dist,
                       get_bucket_idx](std::vector<RoomId> &improved,
                                       const RoomId u_id,
                                       const size_t bucket_idx,
                                       bool lightOnly) {
        const float u_dist = pDists[u_id.asUint32()].load(std::memory_order_relaxed);
        if (get_bucket_idx(u_dist) != bucket_idx) {
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
            const float weight = getLength(e, u_handle, v_handle);
            if (lightOnly && weight > DELTA) {
                continue;
            }
            if (!lightOnly && weight <= DELTA) {
                continue;
            }
            if (relax(v_id,
                      u_dist + weight,
                      u_id,
                      dir,
                      pDists,
                      parents,
                      lastdirs,
                      pLocks,
                      numShards,
                      max_dist)) {
                improved.push_back(v_id);
            }
        }
    };

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
                std::vector<RoomId> improved;
            };
            std::vector<RoomId> all_improved;

            ProgressCounter pc;
            thread_utils::parallel_for_each_tl<TlData>(
                current_nodes,
                pc,
                [&relax_node, current_bucket_idx](TlData &tl, const RoomId u_id) {
                    relax_node(tl.improved, u_id, current_bucket_idx, true);
                },
                [&all_improved](auto &tls) {
                    for (auto &tl : tls) {
                        all_improved.insert(all_improved.end(),
                                            tl.improved.begin(),
                                            tl.improved.end());
                    }
                });

            for (const RoomId v_id : all_improved) {
                size_t b = get_bucket_idx(pDists[v_id.asUint32()].load());
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
            std::vector<RoomId> improved;
        };
        std::vector<RoomId> all_improved_heavy;

        ProgressCounter pc_heavy;
        thread_utils::parallel_for_each_tl<TlDataHeavy>(
            bucket_nodes,
            pc_heavy,
            [&relax_node, current_bucket_idx](TlDataHeavy &tl, const RoomId u_id) {
                relax_node(tl.improved, u_id, current_bucket_idx, false);
            },
            [&all_improved_heavy](auto &tls) {
                for (auto &tl : tls) {
                    all_improved_heavy.insert(all_improved_heavy.end(),
                                              tl.improved.begin(),
                                              tl.improved.end());
                }
            });

        for (const RoomId v_id : all_improved_heavy) {
            size_t b = get_bucket_idx(pDists[v_id.asUint32()].load());
            if (buckets.size() <= b) {
                buckets.resize(b + 1);
            }
            buckets[b].push_back(v_id);
        }

        std::vector<RoomId> targets_in_bucket;
        for (const RoomId id : bucket_nodes) {
            if (targets.contains(id)
                && get_bucket_idx(pDists[id.asUint32()].load()) == current_bucket_idx) {
                targets_in_bucket.push_back(id);
            }
        }

        if (!targets_in_bucket.empty()) {
            std::sort(targets_in_bucket.begin(),
                      targets_in_bucket.end(),
                      [pDists](RoomId a, RoomId b) {
                          return pDists[a.asUint32()].load() < pDists[b.asUint32()].load();
                      });
            for (const RoomId target_id : targets_in_bucket) {
                ShortestPathResult result;
                result.id = target_id;
                result.dist = static_cast<double>(pDists[target_id.asUint32()].load());
                RoomId curr = target_id;
                while (curr != origin_id) {
                    result.path.push_back(lastdirs.at(curr));
                    curr = parents.at(curr);
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
