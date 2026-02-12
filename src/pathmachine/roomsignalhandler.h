#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "../map/ChangeList.h"
#include "../map/DoorFlags.h"
#include "../map/ExitDirection.h"
#include "../map/ExitFieldVariant.h"
#include "../map/ExitFlags.h"
#include "../map/roomid.h"

#include <map>
#include <set>

#include <QObject>
#include <QString>

class PathProcessor;
struct RoomId;
class MapFrontend;

/*!
 * @brief Manages room lifecycle signals and "holds" during pathfinding.
 *
 * Owned by PathMachine, it queues changes to a ChangeList rather than applying
 * them directly.
 */
class NODISCARD_QOBJECT RoomSignalHandler final : public QObject
{
    Q_OBJECT

private:
    MapFrontend &m_map;
    RoomIdSet m_owners;
    std::map<RoomId, size_t> m_holdCount;

public:
    RoomSignalHandler() = delete;
    explicit RoomSignalHandler(MapFrontend &map, QObject *parent)
        : QObject(parent)
        , m_map{map}
    {}

    void hold(RoomId room);
    void release(RoomId room);
    void keep(RoomId room, ExitDirEnum dir, RoomId fromId, ChangeList &changes);

    NODISCARD size_t getNumHolders(RoomId room) const;
};
