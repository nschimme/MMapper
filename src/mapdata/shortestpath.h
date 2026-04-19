#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: 'Elval' <ethorondil@gmail.com> (Elval)

#include "../map/ExitDirection.h"
#include "../map/roomid.h"

#include <cstdint>
#include <vector>

class Map;

struct NODISCARD ShortestPathResult final
{
    RoomId id;
    double dist = 0.0;
    std::vector<ExitDirEnum> path;
};

class NODISCARD ShortestPathRecipient
{
public:
    virtual ~ShortestPathRecipient();

private:
    virtual void virt_receiveShortestPath(const Map &map, ShortestPathResult result) = 0;

public:
    void receiveShortestPath(const Map &map, ShortestPathResult result)
    {
        virt_receiveShortestPath(map, std::move(result));
    }
};
