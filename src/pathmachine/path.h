#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Ulf Hermann <ulfonk_mennhar@gmx.de> (Alve)
// Author: Marek Krejza <krejza@gmail.com> (Caligor)
// Author: Nils Schimmelmann <nschimme@gmail.com> (Jahara)

#include "../global/Badge.h"
#include "../map/DoorFlags.h"
#include "../map/ExitDirection.h"
#include "../map/ExitFieldVariant.h"
#include "../map/ExitFlags.h"
#include "../map/RoomHandle.h"
#include "../map/ChangeList.h" // Added for ChangeList
#include "pathparameters.h"

#include <cassert>
#include <climits>
#include <list>
#include <memory>
#include <optional>
#include <vector>

#include <QtGlobal>

class Coordinate;
// class RoomRecipient; // Replaced by PathProcessor
#include "PathProcessor.h" // PathProcessor is now in the same directory
class RoomSignalHandler;
struct PathParameters;

class NODISCARD Path final : public std::enable_shared_from_this<Path>
{
private:
    std::shared_ptr<Path> m_parent;
    std::vector<std::weak_ptr<Path>> m_children;
    double m_probability = 1.0;
    // in fact a path only has one room, one parent and some children (forks).
    const RoomHandle m_room;
    RoomSignalHandler &m_signaler; // Changed to reference
    const std::optional<ExitDirEnum> m_dir;
    bool m_zombie = false;

public:
    static std::shared_ptr<Path> alloc(const RoomHandle &room,
                                       std::weak_ptr<PathProcessor> locker_handle, // Changed to std::weak_ptr
                                       RoomSignalHandler &signaler, // Changed to reference
                                       std::optional<ExitDirEnum> direction);

public:
    explicit Path(Badge<Path>,
                  RoomHandle moved_room,
                  std::weak_ptr<PathProcessor> locker_handle, // Changed to std::weak_ptr
                  RoomSignalHandler &signaler, // Changed to reference
                  std::optional<ExitDirEnum> direction);
    DELETE_CTORS_AND_ASSIGN_OPS(Path);

    void insertChild(const std::shared_ptr<Path> &p);
    void removeChild(const std::shared_ptr<Path> &p);
    void setParent(const std::shared_ptr<Path> &p);
    NODISCARD bool hasChildren() const
    {
        assert(!m_zombie);
        return !m_children.empty();
    }
    NODISCARD RoomHandle getRoom() const
    {
        assert(!m_zombie);
        return m_room;
    }

    // new Path is created, distance between rooms is calculated and probability is set accordingly
    NODISCARD std::shared_ptr<Path> fork(const RoomHandle &room,
                                         const Coordinate &expectedCoordinate,
                                         const PathParameters &params,
                                         std::weak_ptr<PathProcessor> locker_handle, // Changed to std::weak_ptr
                                         ExitDirEnum dir);
    NODISCARD double getProb() const
    {
        assert(!m_zombie);
        return m_probability;
    }
    void approve(ChangeList &changes);

    // deletes this path and all parents up to the next branch
    void deny(ChangeList &changes);
    void setProb(double p)
    {
        assert(!m_zombie);
        m_probability = p;
    }

    NODISCARD const std::shared_ptr<Path> &getParent() const
    {
        assert(!m_zombie);
        return m_parent;
    }

private:
    double calculateInitialScoreFactor(const RoomHandle &current_room_handle,
                                       const RoomHandle &next_room_handle,
                                       const Coordinate &expected_next_coord,
                                       ExitDirEnum direction_to_next_room,
                                       const PathParameters &params) const;

    double applyPathPenalties(double current_score_factor,
                              const RoomHandle &next_room_handle,
                              const PathParameters &params) const;
};

struct NODISCARD PathList : public std::list<std::shared_ptr<Path>>,
                            public std::enable_shared_from_this<PathList>
{
public:
    NODISCARD static std::shared_ptr<PathList> alloc()
    {
        return std::make_shared<PathList>(Badge<PathList>{});
    }

public:
    explicit PathList(Badge<PathList>) {}
    DELETE_CTORS_AND_ASSIGN_OPS(PathList);
};
