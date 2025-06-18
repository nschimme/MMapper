// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Ulf Hermann <ulfonk_mennhar@gmx.de> (Alve)
// Author: Marek Krejza <krejza@gmail.com> (Caligor)

#include "approved.h"

#include "../map/Compare.h"
#include "../map/room.h"
#include "../mapdata/mapdata.h"

Approved::Approved(MapFrontend &map, const SigParseEvent &sigParseEvent, const int tolerance)
    : myEvent{sigParseEvent.requireValid()}
    , m_map{map}
    , matchingTolerance{tolerance}

{}

Approved::~Approved()
{
    if (matchedRoom) {
        if (moreThanOne) {
            m_map.releaseRoom(this, matchedRoom.getId());
        } else {
            m_map.keepRoom(this, matchedRoom.getId());
        }
    }
}

void Approved::receiveRoom(const RoomHandle &perhaps)
{
    auto &event = myEvent.deref();

    const auto id = perhaps.getId();
    const auto cmp = [this, &event, &id, &perhaps]() {
        // Cache comparisons because we regularly call releaseMatch() and try the same rooms again
        auto it = compareCache.find(id);
        if (it != compareCache.end()) {
            return it->second;
        }
        const auto result = ::compare(perhaps.getRaw(), event, matchingTolerance);
        compareCache.emplace(id, result);
        return result;
    }();

    if (cmp == ComparisonResultEnum::DIFFERENT) {
        // Since 'this' is the locker, and we are deciding not to keep this room,
        // we don't need to call releaseRoom on m_map here as it was never "kept" by this instance yet.
        // m_map.releaseRoom(this, id); // This line is removed.
        return;
    }

    if (matchedRoom) {
        // moreThanOne should only take effect if multiple distinct rooms match
        if (matchedRoom.getId() != id) {
            moreThanOne = true;
        }
        // If another room was already matched, we don't keep this new one.
        // m_map.releaseRoom(this, id); // This line is removed.
        return;
    }

    matchedRoom = perhaps;
    if (cmp == ComparisonResultEnum::TOLERANCE
        && (event.hasNameDescFlags() || event.hasServerId())) {
        update = true;
    } else if (cmp == ComparisonResultEnum::EQUAL) {
        for (const ExitDirEnum dir : ALL_EXITS_NESWUD) {
            const auto toServerId = event.getExitIds()[dir];
            if (toServerId == INVALID_SERVER_ROOMID) {
                continue;
            }
            const auto &e = matchedRoom.getExit(dir);
            if (e.exitIsNoMatch()) {
                continue;
            }
            const auto there = m_map.findRoomHandle(toServerId);
            if (!there && !e.exitIsUnmapped()) {
                // New server id
                update = true;
            } else if (there && !e.containsOut(there.getId())) {
                // Existing server id
                update = true;
            }
        }
    }
}

RoomHandle Approved::oneMatch() const
{
    // Note the logic: If there's more than one, then we return nothing.
    return moreThanOne ? RoomHandle{} : matchedRoom;
}

void Approved::releaseMatch()
{
    // Release the current candidate in order to receive additional candidates
    if (matchedRoom) {
        // We are releasing our current match to potentially receive a new one.
        // The actual release from m_map's perspective (if it was kept)
        // will happen when this Approved object is destroyed or if a new room is kept.
        // For now, we just reset our internal state.
        // m_map.releaseRoom(this, matchedRoom.getId()); // This line is removed.
    }
    update = false;
    matchedRoom.reset();
    moreThanOne = false;
}
