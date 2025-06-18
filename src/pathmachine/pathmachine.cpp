// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Ulf Hermann <ulfonk_mennhar@gmx.de> (Alve)
// Author: Marek Krejza <krejza@gmail.com> (Caligor)
// Author: Nils Schimmelmann <nschimme@gmail.com> (Jahara)

#include "pathmachine.h"

#include "../global/logging.h"
#include "../global/utils.h"
#include "../map/ChangeList.h"
#include "../map/ChangeTypes.h"
#include "../map/CommandId.h"
#include "../map/ConnectedRoomFlags.h"
#include "../map/ExitDirection.h"
#include "../map/coordinate.h"
#include "../map/mmapper2room.h"
#include "../map/room.h"
#include "../map/roomid.h"
#include "../mapdata/mapdata.h"
#include "approved.h"
#include "crossover.h"
#include "experimenting.h"
#include "onebyone.h"
#include "path.h"
#include "pathparameters.h"
#include "roomsignalhandler.h"
#include "syncing.h"

#include <cassert>
#include <memory>
#include <optional>
#include <unordered_set>

class RoomRecipient;

PathMachine::PathMachine(MapFrontend &map, QObject *const parent)
    : QObject(parent)
    , m_map{map}
    , m_signaler{map, this}
    , m_lastEvent{ParseEvent::createDummyEvent()}
    , m_paths{PathList::alloc()}
{}

bool PathMachine::hasLastEvent() const
{
    if (!m_lastEvent.isValid()) {
        return false;
    }

    ParseEvent &event = m_lastEvent.deref();
    return event.canCreateNewRoom();
}

void PathMachine::onMapLoaded()
{
    if (hasLastEvent()) {
        handleParseEvent(m_lastEvent);
    }
}

void PathMachine::forcePositionChange(const RoomId id, const bool update)
{
    slot_releaseAllPaths();

    if (id == INVALID_ROOMID) {
        MMLOG() << "in " << __FUNCTION__ << " detected invalid room.";

        clearMostLikelyRoom();
        m_state = PathStateEnum::SYNCING;
        return;
    }

    const auto &room = m_map.findRoomHandle(id);
    if (!room) {
        clearMostLikelyRoom();
        m_state = PathStateEnum::SYNCING;
        return;
    }

    setMostLikelyRoom(id);
    emit sig_playerMoved(id);
    m_state = PathStateEnum::APPROVED;

    if (!update) {
        return;
    }

    if (!hasLastEvent()) {
        MMLOG() << "in " << __FUNCTION__ << " has invalid event.";
        return;
    }

    // Force update room with last event
    ChangeList changes;
    changes.add(Change{room_change_types::Update{id, m_lastEvent.deref(), UpdateTypeEnum::Force}});
    updateMostLikelyRoom(m_lastEvent, changes, true);
    if (!changes.empty()) {
        scheduleAction(changes);
    }
}

void PathMachine::slot_releaseAllPaths()
{
    auto &paths = deref(m_paths);
    for (auto &path : paths) {
        path->deny();
    }
    paths.clear();

    m_state = PathStateEnum::SYNCING;

    // REVISIT: should these be cleared, too?
    if ((false)) {
        m_pathRoot.reset();
        m_mostLikelyRoom.reset();
    }
}

void PathMachine::handleParseEvent(const SigParseEvent &sigParseEvent)
{
    if (m_lastEvent != sigParseEvent.requireValid()) {
        m_lastEvent = sigParseEvent;
    }

    m_lastEvent.requireValid();

    ChangeList changes;

    switch (m_state) {
    case PathStateEnum::APPROVED:
        approved(sigParseEvent, changes);
        break;
    case PathStateEnum::EXPERIMENTING:
        experimenting(sigParseEvent, changes);
        break;
    case PathStateEnum::SYNCING:
        syncing(sigParseEvent, changes);
        break;
    }

    if (m_state == PathStateEnum::APPROVED && hasMostLikelyRoom()) {
        updateMostLikelyRoom(sigParseEvent, changes, false);
    }
    if (!changes.empty()) {
        scheduleAction(changes);
    }
    if (m_state != PathStateEnum::SYNCING) {
        if (const auto room = getMostLikelyRoom()) {
            emit sig_playerMoved(room.getId());
        }
    }
}

RoomIdSet PathMachine::tryExits(const RoomHandle &room, const ParseEvent &event, const bool out) {
    RoomIdSet found_ids;
    if (!room.exists()) {
        return found_ids;
    }

    const CommandEnum move = event.getMoveType();
    if (isDirection7(move)) { // ExitDirEnum::UNKNOWN is also a "direction"
        const auto& possible_exit = room.getExit(getDirection(move));
        for (RoomId target_id : (out ? possible_exit.getOutgoingSet() : possible_exit.getIncomingSet())) {
            if (m_map.findRoomHandle(target_id).exists()) { // Validate room existence via MapFrontend
                found_ids.insert(target_id);
            }
        }
    } else {
        // For LOOK, FLEE, SCOUT, NONE
        if (move == CommandEnum::LOOK) { // Special case for LOOK in original tryExits
            found_ids.insert(room.getId());
        } else if (move >= CommandEnum::FLEE) {
            // In the original code, room.getExits() returns a std::array<RawExit, MAX_EXITS>
            // Iterating this gives RawExit objects.
            for (const auto& exit_obj : room.getExits()) {
                for (RoomId target_id : (out ? exit_obj.getOutgoingSet() : exit_obj.getIncomingSet())) {
                    if (m_map.findRoomHandle(target_id).exists()) {
                        found_ids.insert(target_id);
                    }
                }
            }
        }
    }
    return found_ids;
}

// Old PathMachine::tryExit removed as it's no longer used by the new tryExits

RoomIdSet PathMachine::tryCoordinate(const RoomHandle &room, const ParseEvent &event) {
    RoomIdSet found_ids;
    if (!room.exists()) {
        return found_ids;
    }

    const CommandEnum moveCode = event.getMoveType();
    if (moveCode < CommandEnum::FLEE) { // LOOK, UNKNOWN, N, E, S, W, U, D
        Coordinate offset = ::exitDir(getDirection(moveCode)); // getDirection handles UNKNOWN to ExitDirEnum::UNKNOWN (offset 0,0,0)
        const Coordinate target_coord = room.getPosition() + offset;
        if (const auto target_room = m_map.findRoomHandle(target_coord)) {
            if(target_room.exists()) found_ids.insert(target_room.getId());
        }
    } else { // FLEE, SCOUT, NONE
        const Coordinate roomPos = room.getPosition();
        for (const ExitDirEnum dir : ALL_EXITS7) { // ALL_EXITS7 includes UNKNOWN
            const Coordinate target_coord = roomPos + ::exitDir(dir);
            if (const auto target_room = m_map.findRoomHandle(target_coord)) {
                if(target_room.exists()) found_ids.insert(target_room.getId());
            }
        }
    }
    return found_ids;
}

void PathMachine::approved(const SigParseEvent &sigParseEvent, ChangeList &changes)
{
    ParseEvent &event = sigParseEvent.deref();

    RoomHandle perhaps; // Keep this for storing the final match
    Approved appr{m_map, sigParseEvent, m_params.matchingTolerance};

    if (event.hasServerId()) {
        if (const auto rh = m_map.findRoomHandle(event.getServerId())) {
            // Pass RoomHandle directly, Approved::receiveRoom now handles matching criteria
            appr.receiveRoom(rh);
        }
        perhaps = appr.oneMatch(); // Check if server ID alone was sufficient
    }

    if (!perhaps.exists()) { // If no server ID or server ID did not lead to a unique approved match
        appr.releaseMatch(); // Clear any rooms collected from server ID check if it wasn't unique
        RoomIdSet exit_ids_out = tryExits(getMostLikelyRoom(), event, true);
        for (RoomId id : exit_ids_out) {
            if (const auto rh = m_map.findRoomHandle(id)) { appr.receiveRoom(rh); }
        }
        perhaps = appr.oneMatch();

        if (!perhaps.exists()) {
            appr.releaseMatch();
            RoomIdSet exit_ids_in = tryExits(getMostLikelyRoom(), event, false);
            for (RoomId id : exit_ids_in) {
                if (const auto rh = m_map.findRoomHandle(id)) { appr.receiveRoom(rh); }
            }
            perhaps = appr.oneMatch();

            if (!perhaps.exists()) {
                appr.releaseMatch();
                RoomIdSet coord_ids = tryCoordinate(getMostLikelyRoom(), event);
                for (RoomId id : coord_ids) {
                    if (const auto rh = m_map.findRoomHandle(id)) { appr.receiveRoom(rh); }
                }
                perhaps = appr.oneMatch();

                if (!perhaps.exists()) {
                    appr.releaseMatch();
                    const auto cmd = event.getMoveType();
                    const Coordinate &eDir = ::exitDir(getDirection(cmd));
                    if (eDir.z == 0) { // Only for NESW and UNKNOWN moves
                        if (const auto &pos_opt = tryGetMostLikelyRoomPosition()) {
                            Coordinate c = pos_opt.value() + eDir;

                            // Try one step below
                            c.z -= 1;
                            if (const auto rh_below = m_map.findRoomHandle(c)) {
                                appr.receiveRoom(rh_below);
                            }
                            perhaps = appr.oneMatch();

                            if (!perhaps.exists()) {
                                appr.releaseMatch(); // Clear results from trying 'c' (below)
                                // Try one step above
                                c.z += 2; // From one below to one above original
                                if (const auto rh_above = m_map.findRoomHandle(c)) {
                                    appr.receiveRoom(rh_above);
                                }
                                perhaps = appr.oneMatch();
                            }
                        }
                    }
                }
            }
        }
    }

    if (!perhaps.exists()) { // Check if perhaps is still invalid after all attempts
        // couldn't match, give up
        m_state = PathStateEnum::EXPERIMENTING;
        m_pathRoot = m_mostLikelyRoom;

        const RoomHandle pathRoot = getPathRoot();
        if (!pathRoot) {
            // What do we do now? "Who cares?"
            return;
        }

        /* FIXME: null locker ends up being an error in RoomSignalHandler::keep(),
         * so why is this allowed to be null, and how do we prevent this null
         * from actually causing an error? */
        deref(m_paths).push_front(Path::alloc(pathRoot, nullptr, &m_signaler, std::nullopt));
        experimenting(sigParseEvent, changes);

        return;
    }

    // Update the exit from the previous room to the current room
    const CommandEnum move = event.getMoveType();
    if (getMapMode() == MapModeEnum::MAP && isDirectionNESWUD(move)) {
        if (const auto room = getMostLikelyRoom()) {
            const auto dir = getDirection(move);
            const auto &ex = room.getExit(dir);
            const auto to = perhaps.getId();
            const auto toServerId = event.getExitIds()[opposite(dir)];
            if (toServerId != room.getServerId() && !ex.containsOut(to)) {
                const auto from = room.getId();
                changes.add(Change{exit_change_types::ModifyExitConnection{ChangeTypeEnum::Add,
                                                                           from,
                                                                           dir,
                                                                           to,
                                                                           WaysEnum::OneWay}});
            }
        }
    }

    // Update most likely room with player's current location
    setMostLikelyRoom(perhaps.getId());

    if (appr.needsUpdate()) {
        changes.add(Change{room_change_types::Update{perhaps.getId(),
                                                     sigParseEvent.deref(),
                                                     UpdateTypeEnum::Update}});
    }
}

void PathMachine::updateMostLikelyRoom(const SigParseEvent &sigParseEvent,
                                       ChangeList &changes,
                                       bool force)
{
    ParseEvent &event = sigParseEvent.deref();

    // guaranteed to succeed, since it's set above.
    const auto here = getMostLikelyRoom();

    // track added ServerIds to prevent multiple allocations
    std::unordered_set<ServerRoomId> addedIds;

    if (event.hasServerId()) {
        const auto oldId = here.getServerId();
        const auto newId = event.getServerId();
        if (oldId == INVALID_SERVER_ROOMID && newId != INVALID_SERVER_ROOMID) {
            changes.add(Change{room_change_types::SetServerId{here.getId(), newId}});
            addedIds.emplace(newId);
            qInfo() << "Set server id" << newId.asUint32();
        }
    }

    ExitsFlagsType eventExitsFlags = event.getExitsFlags();
    if (eventExitsFlags.isValid()) {
        for (const ExitDirEnum dir : ALL_EXITS_NESWUD) {
            const auto from = here.getId();
            const auto toServerId = event.getExitIds()[dir];
            const auto &roomExit = here.getExit(dir);
            if (roomExit.exitIsNoMatch()) {
                continue;
            }
            if (toServerId == INVALID_SERVER_ROOMID) {
                // Room has a hidden exit or does not agree with event
                if (roomExit.exitIsExit() && !eventExitsFlags.get(dir).isExit()
                    && !roomExit.doorIsHidden()) {
                    if (force) {
                        // Be destructive only on forcing an update
                        changes.add(
                            Change{exit_change_types::NukeExit{from, dir, WaysEnum::OneWay}});
                    } else if (roomExit.exitIsDoor()) {
                        // Map is old and needs hidden flag
                        changes.add(Change{
                            exit_change_types::SetDoorFlags{FlagChangeEnum::Add,
                                                            from,
                                                            dir,
                                                            DoorFlags{DoorFlagEnum::HIDDEN}}});
                    } else {
                        // Use NO_MATCH as a hint to the user which exit isn't matching
                        changes.add(Change{
                            exit_change_types::SetExitFlags{FlagChangeEnum::Add,
                                                            from,
                                                            dir,
                                                            ExitFlags{ExitFlagEnum::NO_MATCH}}});
                    }
                }
                continue;
            }
            if (const auto there = m_map.findRoomHandle(toServerId)) {
                // ServerId already exists
                const auto to = there.getId();
                if ((getMapMode() == MapModeEnum::MAP || force) && !roomExit.containsOut(to)) {
                    changes.add(Change{exit_change_types::ModifyExitConnection{ChangeTypeEnum::Add,
                                                                               from,
                                                                               dir,
                                                                               to,
                                                                               WaysEnum::OneWay}});
                }
            } else if (roomExit.outIsUnique() && addedIds.find(toServerId) == addedIds.end()) {
                // Add likely ServerId
                const auto to = roomExit.outFirst();
                changes.add(Change{room_change_types::SetServerId{to, toServerId}});
                addedIds.emplace(toServerId);
            }
        }
    }

    const ConnectedRoomFlagsType connectedRoomFlags = event.getConnectedRoomFlags();
    if (eventExitsFlags.isValid()) {
        const auto &eventExits = event.getExits();
        for (const ExitDirEnum dir : ALL_EXITS_NESWUD) {
            auto &roomExit = here.getExit(dir);

            ExitFlags roomExitFlags = roomExit.getExitFlags();
            roomExitFlags.remove(ExitFlagEnum::UNMAPPED);
            ExitFlags eventExitFlags = eventExitsFlags.get(dir);

            const DoorFlags roomDoorFlags = roomExit.getDoorFlags();
            const DoorFlags eventDoorFlags = eventExits.at(dir).getDoorFlags();

            // Replace exit/door flags on a force
            if (force) {
                // ... but take care of the following exceptions
                if (roomExit.exitIsRoad() && !eventExitFlags.isRoad()
                    && connectedRoomFlags.isValid() && connectedRoomFlags.hasDirectSunlight(dir)) {
                    // Prevent orcs/trolls from removing roads/trails if they're sunlit
                    eventExitFlags |= ExitFlagEnum::ROAD;
                }

                changes.add(Change{exit_change_types::SetExitFlags{FlagChangeEnum::Set,
                                                                   here.getId(),
                                                                   dir,
                                                                   eventExitFlags}});
                changes.add(Change{exit_change_types::SetDoorFlags{FlagChangeEnum::Set,
                                                                   here.getId(),
                                                                   dir,
                                                                   eventDoorFlags}});

            } else {
                // Otherwise append exit/door flags
                // REVISIT: What about old roads/climbs that need to be removed?
                if (roomExit.exitIsNoMatch() || !eventExitsFlags.get(dir).isExit()) {
                    continue;
                }

                if (eventExitFlags ^ roomExitFlags) {
                    changes.add(Change{exit_change_types::SetExitFlags{FlagChangeEnum::Add,
                                                                       here.getId(),
                                                                       dir,
                                                                       eventExitFlags}});
                }
                if (eventDoorFlags ^ roomDoorFlags) {
                    changes.add(Change{exit_change_types::SetDoorFlags{FlagChangeEnum::Add,
                                                                       here.getId(),
                                                                       dir,
                                                                       eventDoorFlags}});
                }
            }

            const auto &doorName = eventExits.at(dir).getDoorName();
            if (eventDoorFlags.isHidden() && !doorName.isEmpty()
                && roomExit.getDoorName() != doorName) {
                changes.add(Change{exit_change_types::SetDoorName{here.getId(), dir, doorName}});
            }
        }
    }

    const PromptFlagsType pFlags = event.getPromptFlags();
    if (pFlags.isValid()) {
        const RoomSundeathEnum sunType = here.getSundeathType();
        if (pFlags.isLit() && sunType == RoomSundeathEnum::NO_SUNDEATH
            && here.getLightType() != RoomLightEnum::LIT) {
            changes.add(Change{room_change_types::ModifyRoomFlags{here.getId(),
                                                                  RoomLightEnum::LIT,
                                                                  FlagModifyModeEnum::ASSIGN}});

        } else if (pFlags.isDark() && sunType == RoomSundeathEnum::NO_SUNDEATH
                   && here.getLightType() == RoomLightEnum::UNDEFINED
                   && (connectedRoomFlags.isValid() && connectedRoomFlags.hasAnyDirectSunlight())) {
            // REVISIT: Can be temporarily dark due to night time or magical darkness
            changes.add(Change{room_change_types::ModifyRoomFlags{here.getId(),
                                                                  RoomLightEnum::DARK,
                                                                  FlagModifyModeEnum::ASSIGN}});
        }
    }

    // Update rooms behind exits now that we are certain about our current location
    if (const ConnectedRoomFlagsType crf = event.getConnectedRoomFlags();
        crf.isValid() && (crf.hasAnyDirectSunlight() || crf.isTrollMode())) {
        for (const ExitDirEnum dir : ALL_EXITS_NESWUD) {
            const auto &e = here.getExit(dir);
            if (e.getExitFlags().isNoMatch() || e.outIsEmpty() || !e.outIsUnique()) {
                continue;
            }

            const RoomId to = e.outFirst();
            if (const auto there = m_map.findRoomHandle(to)) {
                const RoomSundeathEnum sunType = there.getSundeathType();
                if (crf.hasDirectSunlight(dir) && sunType != RoomSundeathEnum::SUNDEATH) {
                    changes.add(
                        Change{room_change_types::ModifyRoomFlags{to,
                                                                  RoomSundeathEnum::SUNDEATH,
                                                                  FlagModifyModeEnum::ASSIGN}});
                } else if (crf.isTrollMode() && crf.hasNoDirectSunlight(dir)
                           && sunType != RoomSundeathEnum::NO_SUNDEATH) {
                    changes.add(
                        Change{room_change_types::ModifyRoomFlags{to,
                                                                  RoomSundeathEnum::NO_SUNDEATH,
                                                                  FlagModifyModeEnum::ASSIGN}});
                }
            }
        }
    }
}

void PathMachine::syncing(const SigParseEvent &sigParseEvent, ChangeList &changes)
{
    auto &params = m_params;
    ParseEvent &event = sigParseEvent.deref();
    {
        Syncing sync{params, m_paths, &m_signaler};
        if (event.hasServerId() || event.getNumSkipped() <= params.maxSkipped) {
            // Replace m_map.lookingForRooms(sync, sigParseEvent)
            // Map::getRooms was refactored to return RoomIdSet and take ParseEvent
            // It handles server ID preference and calls global ::getRooms.
            RoomIdSet candidate_ids = m_map.getRooms(event); // event is a ParseEvent&
            for (RoomId id : candidate_ids) {
                if (const auto rh = m_map.findRoomHandle(id)) {
                    sync.receiveRoom(rh); // Syncing class has its own receiveRoom
                }
            }
            // If event has a serverId, Map::getRooms would have prioritized it.
            // If only serverId is of interest for Syncing when present, and no tree search,
            // the logic might be more direct:
            // if (event.hasServerId()) {
            //    if (const auto rh = m_map.findRoomHandle(event.getServerId())) {
            //        sync.receiveRoom(rh);
            //    }
            // } else if (event.getNumSkipped() <= params.maxSkipped) {
            //    RoomIdSet candidate_ids = m_map.getRooms(event); // Call without serverId preference
            //    for (RoomId id : candidate_ids) { ... }
            // }
            // However, Map::getRooms(event) should correctly handle the precedence.
            // The original condition "event.hasServerId() || event.getNumSkipped() <= params.maxSkipped"
            // was about *whether* to look for rooms. If Map::getRooms handles this, it's fine.
            // The condition was on calling lookingForRooms, not internal to it.
            // So, we respect that condition for calling m_map.getRooms.
        }
        m_paths = sync.evaluate();
    }
    evaluatePaths(changes);
}

void PathMachine::experimenting(const SigParseEvent &sigParseEvent, ChangeList &changes)
{
    auto &params = m_params;
    ParseEvent &event = sigParseEvent.deref();
    const CommandEnum moveCode = event.getMoveType();

    // only create rooms if it has a serverId or no properties are skipped and the direction is NESWUD.
    if (event.canCreateNewRoom() && isDirectionNESWUD(moveCode) && hasMostLikelyRoom()) {
        const auto dir = getDirection(moveCode);
        const Coordinate &move = ::exitDir(dir);
        // Crossover constructor was updated to:
        // Crossover(MapFrontend &map, std::shared_ptr<PathList> paths, ExitDirEnum dirCode,
        //           PathParameters &params, const SharedParseEvent &event, RoomSignalHandler *room_signal_handler);
        Crossover exp{m_map, m_paths, dir, params, sigParseEvent, &m_signaler}; // Added sigParseEvent and &m_signaler
        RoomIdSet pathEnds;
        for (const auto &path : deref(m_paths)) {
            const auto &working = path->getRoom();
            const RoomId workingId = working.getId();
            if (!pathEnds.contains(workingId)) {
                qInfo() << "creating RoomId" << workingId.asUint32();
                if (getMapMode() == MapModeEnum::MAP) {
                    m_map.slot_createRoom(sigParseEvent, working.getPosition() + move);
                }
                pathEnds.insert(workingId);
            }
        }
        // Refactored Crossover branch:
        RoomIdSet candidate_ids_crossover = m_map.getRooms(event); // event is sigParseEvent.deref()
        for (RoomId id : candidate_ids_crossover) {
            if (const auto rh = m_map.findRoomHandle(id)) {
                exp.receiveRoom(rh); // Crossover class has its own receiveRoom
            }
        }
        m_paths = exp.evaluate();
    } else {
        // Refactored OneByOne branch:
        OneByOne oneByOne{sigParseEvent, params, &m_signaler};
        PathListPtr new_paths_for_onebyone = PathList::alloc();

        for (const auto &path : deref(m_paths)) {
            const auto &working_room_handle = path->getRoom();
            if (!working_room_handle.exists()) continue;

            oneByOne.addPath(path); // Sets current parent path and clears its internal m_collectedRoomIds

            RoomIdSet exit_ids_out = tryExits(working_room_handle, event, true); // NEW tryExits
            for (RoomId id : exit_ids_out) {
                if (const auto rh = m_map.findRoomHandle(id)) oneByOne.receiveRoom(rh);
            }

            RoomIdSet exit_ids_in = tryExits(working_room_handle, event, false); // NEW tryExits
            for (RoomId id : exit_ids_in) {
                if (const auto rh = m_map.findRoomHandle(id)) oneByOne.receiveRoom(rh);
            }

            RoomIdSet coord_ids = tryCoordinate(working_room_handle, event); // NEW tryCoordinate
            for (RoomId id : coord_ids) {
                if (const auto rh = m_map.findRoomHandle(id)) oneByOne.receiveRoom(rh);
            }

            PathListPtr children_paths = oneByOne.evaluate();
            for(const auto& child_path : *children_paths) {
                new_paths_for_onebyone->push_back(child_path);
            }
        }
        m_paths = new_paths_for_onebyone; // Assign accumulated paths
    }

    evaluatePaths(changes);
}

void PathMachine::evaluatePaths(ChangeList &changes)
{
    auto &paths = deref(m_paths);
    if (paths.empty()) {
        m_state = PathStateEnum::SYNCING;
        return;
    }

    if (const auto room = deref(paths.front()).getRoom()) {
        setMostLikelyRoom(room.getId());
    } else {
        // REVISIT: Should this case set state to SYNCING and then return?
        clearMostLikelyRoom();
    }

    // FIXME: above establishes that the room might not exist,
    // but here we happily assume that it does exist.
    if (paths.size() == 1) {
        m_state = PathStateEnum::APPROVED;
        std::shared_ptr<Path> path = utils::pop_front(paths);
        deref(path).approve(changes);
    } else {
        m_state = PathStateEnum::EXPERIMENTING;
    }
}

void PathMachine::scheduleAction(const ChangeList &action)
{
    if (getMapMode() != MapModeEnum::OFFLINE) {
        m_map.applyChanges(action);
    }
}

RoomHandle PathMachine::getPathRoot() const
{
    if (!m_pathRoot.has_value()) {
        return RoomHandle{};
    }

    return m_map.findRoomHandle(m_pathRoot.value());
}

RoomHandle PathMachine::getMostLikelyRoom() const
{
    if (!m_mostLikelyRoom.has_value()) {
        return RoomHandle{};
    }

    return m_map.findRoomHandle(m_mostLikelyRoom.value());
}

void PathMachine::setMostLikelyRoom(const RoomId roomId)
{
    if (const auto &room = m_map.findRoomHandle(roomId)) {
        m_mostLikelyRoom = roomId;
    } else {
        m_mostLikelyRoom.reset();
    }
}

std::optional<Coordinate> PathMachine::tryGetMostLikelyRoomPosition() const
{
    if (const auto room = getMostLikelyRoom()) {
        return room.getPosition();
    }
    return std::nullopt;
}
