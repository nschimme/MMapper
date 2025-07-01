// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Ulf Hermann <ulfonk_mennhar@gmx.de> (Alve)
// Author: Marek Krejza <krejza@gmail.com> (Caligor)

#include "approved.h"

#include "../map/Compare.h"
#include "../map/room.h"
#include "../mapdata/mapdata.h"
#include "PathContext.h" // Include PathContext.h

Approved::Approved(MapFrontend &map, PathContext &context, const SigParseEvent &sigParseEvent, const int tolerance)
    : m_myEvent{sigParseEvent.requireValid()}
    , m_map{map}
    , m_context{context}
    , m_matchingTolerance{tolerance}
{}

Approved::~Approved()
{
    if (m_matchedRoom) {
        const RoomId id = m_matchedRoom.getId();
        if (m_moreThanOne) {
            // If more than one match, the room (if temporary and created this cycle) should be deleted.
            // Or if it was an existing temp room, it should be deleted.
            m_context.requestDeleteTemporaryRoom(id);
        } else {
            // If it's a single match, make it permanent.
            m_context.requestMakeRoomPermanent(id);
        }
    }
}

void Approved::virt_receiveRoom(const RoomHandle &perhaps)
{
    auto &event = m_myEvent.deref();

    const auto id = perhaps.getId();
    const auto cmp = [this, &event, &id, &perhaps]() {
        // Cache comparisons because we regularly call releaseMatch() and try the same rooms again
        auto it = m_compareCache.find(id);
        if (it != m_compareCache.end()) {
            return it->second;
        }
        // Comparison logic remains the same, uses m_map directly or RawRoom data
        const auto result = ::compare(perhaps.getRaw(), event, m_matchingTolerance);
        m_compareCache.emplace(id, result);
        return result;
    }();

    if (cmp == ComparisonResultEnum::DIFFERENT) {
        m_context.requestDeleteTemporaryRoom(id); // Room is different, request deletion if temporary
        return;
    }

    if (m_matchedRoom) {
        // moreThanOne should only take effect if multiple distinct rooms match
        if (m_matchedRoom.getId() != id) {
            m_moreThanOne = true;
        }
        // Another room matches, so the 'perhaps' room (if temporary) should be marked for deletion.
        // The original m_matchedRoom's fate is determined in the destructor or when releaseMatch is called.
        m_context.requestDeleteTemporaryRoom(id);
        return;
    }

    m_matchedRoom = perhaps;
    if (cmp == ComparisonResultEnum::TOLERANCE
        && (event.hasNameDescFlags() || event.hasServerId())) {
        m_update = true;
    } else if (cmp == ComparisonResultEnum::EQUAL) {
        for (const ExitDirEnum dir : ALL_EXITS_NESWUD) {
            const auto toServerId = event.getExitIds()[dir];
            if (toServerId == INVALID_SERVER_ROOMID) {
                continue;
            }
            const auto &e = m_matchedRoom.getExit(dir);
            if (e.exitIsNoMatch()) {
                continue;
            }
            const auto there = m_map.findRoomHandle(toServerId);
            if (!there && !e.exitIsUnmapped()) {
                // New server id
                m_update = true;
            } else if (there && !e.containsOut(there.getId())) {
                // Existing server id
                m_update = true;
            }
        }
    }
}

RoomHandle Approved::oneMatch() const
{
    // Note the logic: If there's more than one, then we return nothing.
    return m_moreThanOne ? RoomHandle{} : m_matchedRoom;
}

void Approved::releaseMatch()
{
    // Release the current candidate in order to receive additional candidates
    if (m_matchedRoom) {
        // When a match is released, if it was temporary, it should be marked for deletion.
        // Its fate (permanent or delete) will be sealed by new calls to receiveRoom or by the destructor.
        m_context.requestDeleteTemporaryRoom(m_matchedRoom.getId());
    }
    m_update = false;
    m_matchedRoom.reset();
    m_moreThanOne = false;
}
