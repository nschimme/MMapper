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
#include <memory> // For std::shared_ptr

class MapData;
class RoomHandle;
// class ChangeList; // Forward declaration if full include is not desired, but full include is better for parameters

/**
 * @brief Defines an interface for various room processing and pathfinding strategies.
 *
 * PathProcessor is an abstract base class used by PathMachine to delegate
 * the logic of how incoming room data (from parse events or map lookups)
 * should be processed in different pathfinding states (e.g., Approved,
 * Experimenting, Syncing). Concrete subclasses implement specific strategies
 * for matching rooms, evaluating potential paths, or creating new path segments.
 *
 * The primary interface method is virt_receiveRoom(), which is called to pass
 * a RoomHandle (and a ChangeList for queuing map modifications) to the strategy.
 *
 * Concrete PathProcessor strategies are managed via std::shared_ptr (created by
 * PathMachine) and must inherit from std::enable_shared_from_this<TheirConcreteType>.
 * PathProcessor provides a pure virtual interface (getSharedPtrFromThis) that these
 * concrete classes implement (typically by returning their own shared_from_this()).
 * This allows polymorphic retrieval of a std::shared_ptr<PathProcessor>, which is
 * then used to create std::weak_ptr "locker" handles for interaction with
 * RoomSignalHandler (often via Path objects).
 *
 * @note PathProcessor does not inherit from QObject.
 *       This comment block should precede the class definition.
 */
class NODISCARD PathProcessor
{
public:
    PathProcessor();
    virtual ~PathProcessor();

private:
    virtual void virt_receiveRoom(const RoomHandle &, ChangeList &changes) = 0;

public:
    // Interface for obtaining a shared_ptr to the object
    virtual std::shared_ptr<PathProcessor> getSharedPtrFromThis() = 0;
    virtual std::shared_ptr<const PathProcessor> getSharedPtrFromThis() const = 0;

    void receiveRoom(const RoomHandle &room, ChangeList &changes) { virt_receiveRoom(room, changes); }

public:
    DELETE_CTORS_AND_ASSIGN_OPS(PathProcessor);
};
