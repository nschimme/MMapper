#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Ulf Hermann <ulfonk_mennhar@gmx.de> (Alve)
// Author: Marek Krejza <krejza@gmail.com> (Caligor)

#include "../map/DoorFlags.h"
#include "../map/ExitDirection.h"
#include "../map/ExitFieldVariant.h"
#include "../map/ExitFlags.h"
#include "experimenting.h"
#include "path.h"
#include <memory> // Already present, needed for std::enable_shared_from_this

class MapFrontend;
struct PathParameters;

/**
 * @brief PathProcessor strategy for creating paths when new rooms might be formed.
 *
 * Used in "Experimenting" state. Extends multiple existing paths from `m_shortPaths`
 * (inherited from Experimenting) into newly observed or created rooms that match
 * the current event, using `augmentPath()`.
 */
class NODISCARD Crossover final : public Experimenting, public std::enable_shared_from_this<Crossover>
{
private:
    MapFrontend &m_map;

public:
    Crossover(MapFrontend &map,
              std::shared_ptr<PathList> paths,
              ExitDirEnum dirCode,
              PathParameters &params);

private:
    void virt_receiveRoom(const RoomHandle &, ChangeList &changes) final;

public:
    // Overrides for getSharedPtrFromThis
    std::shared_ptr<PathProcessor> getSharedPtrFromThis() override;
    std::shared_ptr<const PathProcessor> getSharedPtrFromThis() const override;
};
