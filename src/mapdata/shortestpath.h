#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: 'Elval' <ethorondil@gmail.com> (Elval)

#include "../map/ExitDirection.h"
#include "../map/roomid.h"

#include <vector>

class Map;

struct NODISCARD SPNode final
{
    RoomId id;
    int parent = -1;
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
                                          int endpoint)
        = 0;

public:
    void receiveShortestPath(const Map &map, const std::vector<SPNode> &spnodes, const int endpoint)
    {
        virt_receiveShortestPath(map, spnodes, endpoint);
    }
};
