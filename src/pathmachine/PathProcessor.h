#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Ulf Hermann <ulfonk_mennhar@gmx.de> (Alve)
// Author: Marek Krejza <krejza@gmail.com> (Caligor)
// Author: Nils Schimmelmann <nschimme@gmail.com> (Jahara)

#include "../global/RuleOf5.h"
#include "../global/macros.h"
#include "../map/ChangeList.h" // Corrected path
#include "../map/roomid.h"     // Corrected path

class MapData;
class RoomHandle;
// class ChangeList; // Forward declaration if full include is not desired, but full include is better for parameters

/*! \brief Interface for processing paths, giving access to room data.
 *
 * This class defines an interface for components that process paths
 * or need to react to room information.
 */
class NODISCARD PathProcessor
{
public:
    PathProcessor();
    virtual ~PathProcessor();

private:
    virtual void virt_receiveRoom(const RoomHandle &, ChangeList &changes) = 0;

public:
    void receiveRoom(const RoomHandle &room, ChangeList &changes) { virt_receiveRoom(room, changes); }

public:
    DELETE_CTORS_AND_ASSIGN_OPS(PathProcessor);
};
