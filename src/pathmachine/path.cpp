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

std::shared_ptr<Path> Path::alloc(const RoomHandle &room,
                                  PathMachine::PathProcessor *const locker, // CHANGED
                                  RoomSignalHandler *const signaler,
                                  std::optional<ExitDirEnum> moved_direction)
{
    return std::make_shared<Path>(Badge<Path>{}, room, locker, signaler, std::move(moved_direction));
}

Path::Path(Badge<Path>,
           RoomHandle moved_room,
           PathMachine::PathProcessor *const locker, // CHANGED
           RoomSignalHandler *const in_signaler,
           std::optional<ExitDirEnum> moved_direction)
    : m_room(std::move(moved_room))
    , m_signaler(in_signaler)
    , m_dir(std::move(moved_direction))
{
    if (m_dir.has_value()) {
        deref(m_signaler).hold(m_room.getId(), locker);
    }
}

double Path::calculateProbabilityScore(const RoomHandle &forkedRoom,
                                       const Coordinate &expectedCoordinate,
                                       ExitDirEnum direction,
                                       const PathParameters &p) const
{
    const auto udir = static_cast<uint32_t>(direction);
    // assert(isClamped(udir, 0u, NUM_EXITS)); // This assert can remain in fork

    double score = expectedCoordinate.distance(forkedRoom.getPosition());

    const auto currentRoomSize = static_cast<uint32_t>(m_room.isTemporary() ? 0 : NUM_EXITS);
    // NOTE: we can probably assert that size is nonzero (room is not a dummy).
    assert(currentRoomSize == 0u /* dummy */ || currentRoomSize == NUM_EXITS /* valid */);

    if (score < 0.5) { // Very close or at the same spot
        if (udir < NUM_EXITS_INCLUDING_NONE) { // NESWUD or NONE (direction of move)
            score = 1.0 / p.correctPositionBonus;
        } else { // UNKNOWN, SPECIAL (direction of move)
            score = p.multipleConnectionsPenalty; // Original code set dist = p.multipleConnectionsPenalty
        }
    } else { // Further away
        if (udir < currentRoomSize) { // Direction of move is a valid exit from current room
            const auto &e = m_room.getExit(direction);
            auto oid = forkedRoom.getId();
            if (e.containsOut(oid)) { // Current room's exit directly leads to forkedRoom
                score = 1.0 / p.correctPositionBonus;
            } else if (!e.outIsEmpty() || oid == m_room.getId()) {
                // Current room's exit leads somewhere else, or forkedRoom is the same as current room (but geometrically distant)
                score *= p.multipleConnectionsPenalty;
            } else {
                // Current room's exit is unmapped. Check reverse exit from forkedRoom.
                const auto &oe = forkedRoom.getExit(opposite(direction));
                if (!oe.inIsEmpty()) { // If forkedRoom has an incoming exit from this direction
                    score *= p.multipleConnectionsPenalty;
                }
                // If both current exit is unmapped AND forkedRoom has no incoming from there, geometric 'score' remains.
            }
        } else if (udir < NUM_EXITS_INCLUDING_NONE) { // Direction of move is NONE (but not UNKNOWN/SPECIAL)
                                                      // This implies we are evaluating the current room or an adjacent one without a specific exit direction
            // Check if any exit from m_room leads to forkedRoom
            bool foundConnection = false;
            for (uint32_t d = 0; d < currentRoomSize; ++d) {
                const auto &e = m_room.getExit(static_cast<ExitDirEnum>(d));
                if (e.containsOut(forkedRoom.getId())) {
                    score = 1.0 / p.correctPositionBonus; // Found a direct connection
                    foundConnection = true;
                    break;
                }
            }
            if (!foundConnection) {
                // If no direct connection, the geometric 'score' remains, possibly penalized further
                // The original code did not have an explicit penalty here if no exit matched when udir was NONE.
                // It just kept the geometric distance.
            }
        }
        // If udir is UNKNOWN or SPECIAL and dist >= 0.5, the geometric 'score' also remains.
    }
    return score;
}

/**
 * new Path is created,
 * distance between rooms is calculated
 * and probability is updated accordingly
 */
std::shared_ptr<Path> Path::fork(const RoomHandle &in_room,
                                 const Coordinate &expectedCoordinate,
                                 const PathParameters &p,
                                 PathMachine::PathProcessor *const locker,
                                 const ExitDirEnum direction)
{
    assert(!m_zombie);
    const auto udir = static_cast<uint32_t>(direction);
    assert(isClamped(udir, 0u, NUM_EXITS)); // Assert can stay here

    auto ret = Path::alloc(in_room, locker, m_signaler, direction);
    ret->setParent(shared_from_this());
    insertChild(ret);

    double score = calculateProbabilityScore(in_room, expectedCoordinate, direction, p);

    // Apply locker penalty
    // getNumLockers should be >= 1 because Path::alloc calls m_signaler->hold for in_room
    double numLockers = static_cast<double>(m_signaler->getNumLockers(in_room.getId()));
    if (numLockers < 1.0) { // Should not happen based on logic in Path::alloc and RSH::hold
        numLockers = 1.0; // Defensive coding to prevent division by zero or inflation of probability
    }
    score /= numLockers;

    if (in_room.isTemporary()) {
        score *= p.newRoomPenalty;
    }

    // Prevent division by zero if score is zero (e.g. perfect match and large bonus)
    // A smaller score means higher probability.
    if (score <= 0.0) { // If score is zero or negative (highly unlikely for physical distances/penalties)
        // Assign a very high probability, or handle as error/max probability
        // For now, if score is 0 (perfect match), probability is effectively infinite.
        // Let's cap it or use a very small number for division.
        // Smallest positive double is DBL_MIN. Dividing by a very small score gives large prob.
        // If current m_probability is already low, this could still be low.
        // Let's ensure score is at least a very small positive number if it was ~0.
        // The original code didn't explicitly guard against dist (score) being zero.
        // If 1.0 / p.correctPositionBonus resulted in 0, that would be an issue.
        // Assuming p.correctPositionBonus is always >= 1.0 makes 1.0/p.correctPositionBonus <= 1.0.
        // If score is 0 from bonuses, it implies a perfect scenario.
        // To prevent division by zero and ensure high probability:
        if (m_probability > 0 && score == 0) {
             // Effectively infinite probability, capped by double limits or set to a very large factor
             // Or, if m_probability is the current path's probability, then this new path is even more likely.
             // Let's assume score represents inverse of likelihood factor.
             // If score is 0, it means it's infinitely likely relative to geometric distance.
             // For now, let's use a very small stand-in for score if it's 0 to make probability very high.
            ret->setProb(m_probability * 1e12); // Arbitrary large multiplier
        } else if (score == 0 && m_probability == 0) {
            ret->setProb(0); // Avoid 0/0
        } else {
             ret->setProb(m_probability / score);
        }
    } else {
        ret->setProb(m_probability / score);
    }

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
void Path::deny()
{
    assert(!m_zombie);

    if (!m_children.empty()) {
        return;
    }
    if (m_dir.has_value()) {
        deref(m_signaler).release(m_room.getId());
    }
    if (const auto &parent = getParent()) {
        parent->removeChild(shared_from_this());
        parent->deny();
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
