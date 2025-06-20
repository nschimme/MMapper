#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Ulf Hermann <ulfonk_mennhar@gmx.de> (Alve)
// Author: Marek Krejza <krejza@gmail.com> (Caligor)

#include "../global/RuleOf5.h"
#include "PathProcessor.h" // Changed path
#include "path.h"
#include <list>
#include <memory> // Already present, needed for std::enable_shared_from_this and std::shared_ptr

#include <QtGlobal>

class RoomSignalHandler;
struct PathParameters;

class NODISCARD Syncing final : public PathProcessor, public std::enable_shared_from_this<Syncing>
{
private:
    RoomSignalHandler &m_signaler; // Prefixed
    PathParameters &m_params; // Prefixed
    const std::shared_ptr<PathList> m_paths; // Prefixed
    // This is not our parent; it's the parent we assign to new objects.
    std::shared_ptr<Path> m_parent; // Prefixed
    uint32_t m_numPaths = 0u; // Prefixed

public:
    // Parameter names 'p', 'paths', 'signaler' in constructor are fine, they are local to the declaration/definition
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

    // Overrides for getSharedPtrFromThis
    std::shared_ptr<PathProcessor> getSharedPtrFromThis() override;
    std::shared_ptr<const PathProcessor> getSharedPtrFromThis() const override;
};
