#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "../global/RuleOf5.h"
#include "../global/Signal2.h"
#include "../map/DoorStateEnum.h"
#include "../map/ExitDirection.h"
#include "../map/roomid.h"
#include "../observer/gameobserver.h"
#include "UboBlocks.h"
#include "UboManager.h"

#include <deque>

namespace Legacy {
class Functions;
}

class NODISCARD GLDoors final
{
private:
    GameObserver &m_observer;
    Legacy::UboManager &m_uboManager;
    Signal2Lifetime m_lifetime;

    struct DoorKey
    {
        ServerRoomId roomId;
        ExitDirEnum dir;

        bool operator==(const DoorKey &other) const
        {
            return roomId == other.roomId && dir == other.dir;
        }
    };

    struct DoorEntry
    {
        DoorKey key;
        DoorStateEnum state;
    };

    std::deque<DoorEntry> m_doors; // FIFO of non-closed doors

public:
    explicit GLDoors(GameObserver &observer, Legacy::UboManager &uboManager);
    ~GLDoors();

    DELETE_CTORS_AND_ASSIGN_OPS(GLDoors);

private:
    void onDoorStateChanged(ServerRoomId id, ExitDirEnum dir, DoorStateEnum state);
    void rebuildUbo(Legacy::Functions &gl);
};
