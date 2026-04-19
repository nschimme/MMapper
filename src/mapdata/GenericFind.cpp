// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2024 The MMapper Authors

#include "GenericFind.h"

#include "../global/Timer.h"
#include "../global/thread_utils.h"
#include "../map/Map.h"
#include "roomfilter.h"

RoomIdSet genericFind(const Map &map, const RoomFilter &f)
{
    DECL_TIMER(t, "genericFind (parallel)");

    struct TlData final
    {
        RoomIdSet result;
    };

    ProgressCounter pc;
    TlData tl_data;

    thread_utils::parallel_for_each_tl<TlData>(
        map.getRooms(),
        pc,
        [&map, &f](TlData &tl, const RoomId id) {
            const auto &room = map.getRawRoom(id);
            if (f.filter(room)) {
                tl.result.insert(id);
            }
        },
        [&tl_data](auto &tls) {
            for (auto &tl : tls) {
                tl_data.result.insertAll(tl.result);
            }
        });

    return tl_data.result;
}
