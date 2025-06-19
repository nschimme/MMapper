// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Ulf Hermann <ulfonk_mennhar@gmx.de> (Alve)
// Author: Marek Krejza <krejza@gmail.com> (Caligor)

#include "path.h"

#include "../global/utils.h"
#include "../map/ExitDirection.h"
#include "../map/coordinate.h"
#include "../map/exit.h"
#include "../map/room.h"
#include "../map/roomid.h"
#include "pathparameters.h"
#include "roomsignalhandler.h"

#include <cassert>
#include <cstdint>
#include <memory>
#include <optional>
#include <utility>

// PathProcessor.h and WeakHandle.h are included via path.h
std::shared_ptr<Path> Path::alloc(const RoomHandle &room,
                                  WeakHandle<PathProcessor> locker_handle, // Changed to WeakHandle
                                  RoomSignalHandler *const signaler,
                                  std::optional<ExitDirEnum> moved_direction)
{
    return std::make_shared<Path>(Badge<Path>{}, room, locker_handle, signaler, std::move(moved_direction));
}

Path::Path(Badge<Path>,
           RoomHandle moved_room,
           WeakHandle<PathProcessor> locker_handle, // Changed to WeakHandle
           RoomSignalHandler *const in_signaler,
           std::optional<ExitDirEnum> moved_direction)
    , m_signaler(in_signaler)
    , m_dir(std::move(moved_direction))
{
    if (m_dir.has_value()) {
        // Pass the WeakHandle directly to hold
        deref(m_signaler).hold(m_room.getId(), locker_handle);
    }
}

double Path::calculateInitialScoreFactor(const RoomHandle &current_room_handle,
                                       const RoomHandle &next_room_handle,
                                       const Coordinate &expected_next_coord,
                                       ExitDirEnum direction_to_next_room,
                                       const PathParameters &params) const
{
    double dist = expected_next_coord.distance(next_room_handle.getPosition());
    const auto size = static_cast<uint32_t>(current_room_handle.isTemporary() ? 0 : NUM_EXITS);
    // NOTE: we can probably assert that size is nonzero (room is not a dummy).
    assert(size == 0u /* dummy */ || size == NUM_EXITS /* valid */);

    const auto udir = static_cast<uint32_t>(direction_to_next_room);

    if (dist < 0.5) {
        if (udir < NUM_EXITS_INCLUDING_NONE) {
            /* NOTE: This is currently always true unless the data is corrupt. */
            dist = 1.0 / params.correctPositionBonus;
        } else {
            dist = params.multipleConnectionsPenalty;
        }
    } else {
        if (udir < size) {
            const auto &e = current_room_handle.getExit(direction_to_next_room);
            auto oid = next_room_handle.getId();
            if (e.containsOut(oid)) {
                dist = 1.0 / params.correctPositionBonus;
            } else if (!e.outIsEmpty() || oid == current_room_handle.getId()) {
                dist *= params.multipleConnectionsPenalty;
            } else {
                const auto &oe = next_room_handle.getExit(opposite(direction_to_next_room));
                if (!oe.inIsEmpty()) {
                    dist *= params.multipleConnectionsPenalty;
                }
            }
        } else if (udir < NUM_EXITS_INCLUDING_NONE) {
            /* NOTE: This is currently always true unless the data is corrupt. */
            for (uint32_t d = 0; d < size; ++d) {
                const auto &e = current_room_handle.getExit(static_cast<ExitDirEnum>(d));
                if (e.containsOut(next_room_handle.getId())) {
                    dist = 1.0 / params.correctPositionBonus;
                    break;
                }
            }
        }
    }
    return dist;
}

double Path::applyPathPenalties(double current_score_factor,
                              const RoomHandle &next_room_handle,
                              const PathParameters &params) const
{
    double score = current_score_factor; // Start with the passed-in score factor
    score /= static_cast<double>(m_signaler->getNumLockers(next_room_handle.getId()));
    if (next_room_handle.isTemporary()) {
        score *= params.newRoomPenalty;
    }
    return score;
}

/**
 * new Path is created,
 * distance between rooms is calculated
 * and probability is updated accordingly
 */
std::shared_ptr<Path> Path::fork(const RoomHandle &in_room, // param name: room
                                 const Coordinate &expectedCoordinate,
                                 const PathParameters &p,      // param name: params
                                 WeakHandle<PathProcessor> locker_handle, // Changed to WeakHandle
                                 const ExitDirEnum direction)   // param name: dir
{
    assert(!m_zombie);
    const auto udir = static_cast<uint32_t>(direction); // udir is from original, keep if needed for alloc
    assert(isClamped(udir, 0u, NUM_EXITS)); // udir related assert

    // Pass the WeakHandle to alloc
    auto ret = Path::alloc(in_room, locker_handle, m_signaler, direction);
    ret->setParent(shared_from_this());
    insertChild(ret);

    // Original parameters to fork: (in_room, expectedCoordinate, p, locker, direction)
    // Parameters to calculateInitialScoreFactor: (current_room_handle, next_room_handle, expected_next_coord, direction_to_next_room, params)
    // Parameters to applyPathPenalties: (current_score_factor, next_room_handle, params)

    double score_factor = calculateInitialScoreFactor(this->m_room,         // current_room_handle
                                                      in_room,            // next_room_handle (was in_room in fork)
                                                      expectedCoordinate, // expected_next_coord
                                                      direction,          // direction_to_next_room (was direction in fork)
                                                      p);                 // params (was p in fork)

    // score_factor at this point is 'dist' before old penalties.
    score_factor = applyPathPenalties(score_factor,
                                      in_room, // next_room_handle
                                      p);      // params

    if (score_factor <= 0.00001) { // Avoid division by zero or very small numbers
        score_factor = 0.00001;    // Effectively a very high penalty
    }
    ret->setProb(m_probability / score_factor);


    return ret;
}

void Path::setParent(const std::shared_ptr<Path> &p)
{
    assert(!m_zombie);
    assert(!deref(p).m_zombie);
    m_parent = p;
}

void Path::approve(ChangeList &changes)
{
    assert(!m_zombie);

    const auto &parent = getParent();
    if (parent == nullptr) {
        assert(!m_dir.has_value());
    } else {
        assert(m_dir.has_value());
        const RoomHandle proom = parent->getRoom();
        const auto pId = !proom.exists() ? INVALID_ROOMID : proom.getId();
        deref(m_signaler).keep(m_room.getId(), m_dir.value(), pId, changes);
        parent->removeChild(this->shared_from_this());
        parent->approve(changes);
    }

    for (const std::weak_ptr<Path> &weak_child : m_children) {
        if (auto child = weak_child.lock()) {
            child->setParent(nullptr);
        }
    }

    // was: `delete this`
    this->m_zombie = true;
}

/** removes this path and all parents up to the next branch
 * and removes the respective rooms if experimental
 */
void Path::deny(ChangeList &changes)
{
    assert(!m_zombie);

    if (!m_children.empty()) {
        return;
    }
    if (m_dir.has_value()) {
        deref(m_signaler).release(m_room.getId(), changes);
    }
    if (const auto &parent = getParent()) {
        parent->removeChild(shared_from_this());
        parent->deny(changes);
    }

    // was: `delete this`
    this->m_zombie = true;
}

void Path::insertChild(const std::shared_ptr<Path> &p)
{
    assert(!m_zombie);
    assert(!deref(p).m_zombie);
    m_children.emplace_back(p);
}

void Path::removeChild(const std::shared_ptr<Path> &p)
{
    assert(!m_zombie);
    assert(!deref(p).m_zombie);

    const auto end = m_children.end();
    const auto it = std::remove_if(m_children.begin(), end, [&p](const auto &weak) {
        if (auto shared = weak.lock()) {
            return shared == p; // remove p
        } else {
            return true; // also remove any expired children
        }
    });
    if (it != end) {
        m_children.erase(it, end);
    }
}
