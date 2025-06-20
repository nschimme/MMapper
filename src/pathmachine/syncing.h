#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Ulf Hermann <ulfonk_mennhar@gmx.de> (Alve)
// Author: Marek Krejza <krejza@gmail.com> (Caligor)

#include "../global/RuleOf5.h"
#include "PathProcessor.h"
#include "path.h"
#include <list>
#include <memory>

#include <QtGlobal>

class RoomSignalHandler;
struct PathParameters;

/**
 * @brief PathProcessor strategy for the "Syncing" pathfinding state.
 *
 * Used when PathMachine has no confident location (e.g., initial state or after
 * losing track). It attempts to find any room in the map that matches the
 * current parse event, creating a new root Path for each potential match.
 * `finalizePaths()` is used for cleanup of its internal dummy parent path.
 */
class NODISCARD Syncing final : public PathProcessor // Removed , public std::enable_shared_from_this<Syncing>
{
private:
    RoomSignalHandler &m_signaler;
    PathParameters &m_params;
    const std::shared_ptr<PathList> m_paths;
    // This is not our parent; it's the parent we assign to new objects.
    std::shared_ptr<Path> m_parent;
    uint32_t m_numPaths = 0u;

public:
    explicit Syncing(PathParameters &p,
                     std::shared_ptr<PathList> paths,
                     RoomSignalHandler &signaler);

public:
    Syncing() = delete;
    DELETE_CTORS_AND_ASSIGN_OPS(Syncing);

private:
    void virt_receiveRoom(const RoomHandle &, ChangeList &changes) final;

public:
    std::shared_ptr<PathList> evaluate();
    void finalizePaths(ChangeList &changes);

};
