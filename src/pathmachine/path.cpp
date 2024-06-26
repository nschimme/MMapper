// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Ulf Hermann <ulfonk_mennhar@gmx.de> (Alve)
// Author: Marek Krejza <krejza@gmail.com> (Caligor)

#include "path.h"

#include "../expandoracommon/coordinate.h"
#include "../expandoracommon/exit.h"
#include "../expandoracommon/room.h"
#include "../global/roomid.h"
#include "../global/utils.h"
#include "../mapdata/ExitDirection.h"
#include "pathparameters.h"
#include "roomsignalhandler.h"

#include <cassert>
#include <cstdint>
#include <memory>

std::shared_ptr<Path> Path::alloc(const Room *const room,
                                  RoomAdmin *const owner,
                                  RoomRecipient *const locker,
                                  RoomSignalHandler *const signaler,
                                  std::optional<ExitDirEnum> moved_direction)
{
    return std::make_shared<Path>(this_is_private{0},
                                  room,
                                  owner,
                                  locker,
                                  signaler,
                                  std::move(moved_direction));
}

Path::Path(this_is_private,
           const Room *const in_room,
           RoomAdmin *const owner,
           RoomRecipient *const locker,
           RoomSignalHandler *const in_signaler,
           std::optional<ExitDirEnum> moved_direction)
    : m_room(in_room)
    , m_signaler(in_signaler)
    , m_dir(std::move(moved_direction))
{
    if (m_dir.has_value()) {
        m_signaler->hold(m_room, owner, locker);
    }
}

/**
 * new Path is created,
 * distance between rooms is calculated
 * and probability is updated accordingly
 */
std::shared_ptr<Path> Path::fork(const Room *const in_room,
                                 const Coordinate &expectedCoordinate,
                                 RoomAdmin *const owner,
                                 const PathParameters &p,
                                 RoomRecipient *const locker,
                                 const ExitDirEnum direction)
{
    assert(!m_zombie);

    auto ret = Path::alloc(in_room, owner, locker, m_signaler, direction);
    assert(isClamped(static_cast<uint32_t>(direction), 0u, NUM_EXITS));

    ret->setParent(shared_from_this());
    insertChild(ret);

    double dist = expectedCoordinate.distance(in_room->getPosition());
    const auto size = static_cast<uint>(m_room->getExitsList().size());
    // NOTE: we can probably assert that size is nonzero (room is not a dummy).
    assert(size == 0u /* dummy */ || size == NUM_EXITS /* valid */);

    if (dist < 0.5) {
        if (static_cast<uint>(direction) < NUM_EXITS_INCLUDING_NONE) {
            /* NOTE: This is currently always true unless the data is corrupt. */
            dist = 1.0 / p.correctPositionBonus;
        } else {
            dist = p.multipleConnectionsPenalty;
        }
    } else {
        if (static_cast<uint>(direction) < size) {
            const Exit &e = m_room->exit(direction);
            auto oid = in_room->getId();
            if (e.containsOut(oid)) {
                dist = 1.0 / p.correctPositionBonus;
            } else if (!e.outIsEmpty() || oid == m_room->getId()) {
                dist *= p.multipleConnectionsPenalty;
            } else {
                const Exit &oe = in_room->exit(opposite(direction));
                if (!oe.inIsEmpty()) {
                    dist *= p.multipleConnectionsPenalty;
                }
            }

        } else if (static_cast<uint>(direction) < NUM_EXITS_INCLUDING_NONE) {
            /* NOTE: This is currently always true unless the data is corrupt. */
            for (uint d = 0; d < size; ++d) {
                const Exit &e = m_room->exit(static_cast<ExitDirEnum>(d));
                if (e.containsOut(in_room->getId())) {
                    dist = 1.0 / p.correctPositionBonus;
                    break;
                }
            }
        }
    }
    dist /= static_cast<double>(m_signaler->getNumLockers(in_room));
    if (in_room->isTemporary()) {
        dist *= p.newRoomPenalty;
    }
    ret->setProb(m_probability / dist);

    return ret;
}

void Path::setParent(const std::shared_ptr<Path> &p)
{
    assert(!m_zombie);
    assert(!p->m_zombie);
    m_parent = p;
}

void Path::approve()
{
    assert(!m_zombie);

    const auto &parent = getParent();
    if (parent == nullptr) {
        assert(!m_dir.has_value());
    } else {
        assert(m_dir.has_value());
        const Room *const proom = parent->getRoom();
        const auto pId = (proom == nullptr) ? INVALID_ROOMID : proom->getId();
        m_signaler->keep(m_room, m_dir.value(), pId);
        parent->removeChild(this->shared_from_this());
        parent->approve();
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
        m_signaler->release(m_room);
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
    assert(!p->m_zombie);
    m_children.emplace_back(p);
}

void Path::removeChild(const std::shared_ptr<Path> &p)
{
    assert(!m_zombie);
    assert(!p->m_zombie);

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
