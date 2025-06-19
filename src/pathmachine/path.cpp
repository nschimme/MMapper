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
#include "../map/Change.h" // Added Change.h
#include "../map/ChangeTypes.h" // Added ChangeTypes.h
#include "pathparameters.h"
// #include "roomsignalhandler.h" // Removed

#include <cassert>
#include <cstdint>
#include <memory>
#include <optional>
#include <utility>

std::shared_ptr<Path> Path::alloc(const RoomHandle &room,
                                  // RoomRecipient *const locker, // Removed parameter
                                  // RoomSignalHandler *const signaler, // Removed parameter
                                  std::optional<ExitDirEnum> moved_direction)
{
    return std::make_shared<Path>(Badge<Path>{}, room, /*locker, signaler,*/ std::move(moved_direction));
}

Path::Path(Badge<Path>,
           RoomHandle moved_room,
           // RoomRecipient *const locker, // Removed parameter
           // RoomSignalHandler *const in_signaler, // Removed parameter
           std::optional<ExitDirEnum> moved_direction)
    : m_room(std::move(moved_room))
    // , m_signaler(in_signaler) // Removed member initialization
    , m_dir(std::move(moved_direction))
{
    // if (m_dir.has_value()) { // Removed block
    //     deref(m_signaler).hold(m_room.getId(), locker);
    // }
}

/**
 * new Path is created,
 * distance between rooms is calculated
 * and probability is updated accordingly
 */
std::shared_ptr<Path> Path::fork(const RoomHandle &in_room,
                                 const Coordinate &expectedCoordinate,
                                 const PathParameters &p,
                                 // RoomRecipient *const locker, // Removed parameter
                                 const ExitDirEnum direction)
{
    assert(!m_zombie);
    const auto udir = static_cast<uint32_t>(direction);
    assert(isClamped(udir, 0u, NUM_EXITS));

    auto ret = Path::alloc(in_room, /*locker, m_signaler,*/ direction); // Updated alloc call
    ret->setParent(shared_from_this());
    insertChild(ret);

    double dist = expectedCoordinate.distance(in_room.getPosition());
    const auto size = static_cast<uint32_t>(m_room.isTemporary() ? 0 : NUM_EXITS);
    // NOTE: we can probably assert that size is nonzero (room is not a dummy).
    assert(size == 0u /* dummy */ || size == NUM_EXITS /* valid */);

    if (dist < 0.5) {
        if (udir < NUM_EXITS_INCLUDING_NONE) {
            /* NOTE: This is currently always true unless the data is corrupt. */
            dist = 1.0 / p.correctPositionBonus;
        } else {
            dist = p.multipleConnectionsPenalty;
        }
    } else {
        if (udir < size) {
            const auto &e = m_room.getExit(direction);
            auto oid = in_room.getId();
            if (e.containsOut(oid)) {
                dist = 1.0 / p.correctPositionBonus;
            } else if (!e.outIsEmpty() || oid == m_room.getId()) {
                dist *= p.multipleConnectionsPenalty;
            } else {
                const auto &oe = in_room.getExit(opposite(direction));
                if (!oe.inIsEmpty()) {
                    dist *= p.multipleConnectionsPenalty;
                }
            }

        } else if (udir < NUM_EXITS_INCLUDING_NONE) {
            /* NOTE: This is currently always true unless the data is corrupt. */
            for (uint32_t d = 0; d < size; ++d) {
                const auto &e = m_room.getExit(static_cast<ExitDirEnum>(d));
                if (e.containsOut(in_room.getId())) {
                    dist = 1.0 / p.correctPositionBonus;
                    break;
                }
            }
        }
    }
    // TODO: The removal of the following penalty (division by numLockers)
    // might affect pathfinding logic, especially in dense areas or where multiple
    // paths might consider the same room. This needs to be evaluated and potentially
    // compensated for elsewhere if path quality degrades.
    // dist /= static_cast<double>(m_signaler->getNumLockers(in_room.getId()));
    if (in_room.isTemporary()) {
        dist *= p.newRoomPenalty;
    }
    ret->setProb(m_probability / dist);

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
        // New logic for generating changes
        if (m_room.isTemporary()) {
            changes.add(Change{room_change_types::MakePermanent{m_room.getId()}});
        }

        const RoomHandle proom = parent->getRoom(); // Parent is guaranteed by m_dir.has_value() check earlier
        const auto pId = proom.getId(); // Assuming proom exists if parent exists.

        if (pId != INVALID_ROOMID && m_dir.has_value()) { // Ensure parent room ID is valid
             changes.add(Change{exit_change_types::ModifyExitConnection{
                ChangeTypeEnum::Add, pId, m_dir.value(), m_room.getId(), WaysEnum::OneWay}});
        }
        // End of new logic

        // Original logic for parent recursion
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
    // if (m_dir.has_value()) { // Removed block
    //     deref(m_signaler).release(m_room.getId());
    // }
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
