#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: 'Elval' <ethorondil@gmail.com> (Elval)

#include "../map/ExitDirection.h"
#include "../map/roomid.h"

#include <cstdint>
#include <limits>
#include <vector>

class Map;

using SPNodeIdx = std::uint32_t;
static constexpr const SPNodeIdx INVALID_SPNODE_IDX = std::numeric_limits<SPNodeIdx>::max();

struct NODISCARD SPNode final
{
    RoomId id;
    SPNodeIdx parent = INVALID_SPNODE_IDX;
    double dist = 0.0;
    ExitDirEnum lastdir = ExitDirEnum::NONE;
};

class NODISCARD ShortestPathRecipient
{
public:
    virtual ~ShortestPathRecipient();

private:
    virtual void virt_receiveShortestPath(const Map &map,
                                          const std::vector<SPNode> &spnodes,
                                          SPNodeIdx endpoint)
        = 0;

public:
    void receiveShortestPath(const Map &map,
                             const std::vector<SPNode> &spnodes,
                             const SPNodeIdx endpoint)
    {
        virt_receiveShortestPath(map, spnodes, endpoint);
    }
};
