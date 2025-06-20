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

class PathProcessor; // Changed from RoomRecipient

PathMachine::PathMachine(MapFrontend &map, QObject *const parent)
    : QObject(parent)
    , m_map{map}
    , m_signaler{map, this}
    , m_lastEvent{ParseEvent::createDummyEvent()}
    , m_paths{PathList::alloc()}
{}

void PathMachine::findAndReceiveRooms(RoomId targetRoomId, PathProcessor &recipient, ChangeList &changes)
{
    // This method doesn't directly use 'changes' parameter itself, but recipient.receiveRoom might.
    // For now, 'changes' is passed along.
    RoomIdSet ids = m_map.lookingForRooms(targetRoomId);
    for (RoomId id : ids) {
        if (auto rh = m_map.findRoomHandle(id)) {
            // Assuming PathProcessor::receiveRoom now takes ChangeList
            recipient.receiveRoom(rh, changes);
        }
    }
}

void PathMachine::findAndReceiveRooms(const Coordinate &targetCoord, PathProcessor &recipient, ChangeList &changes)
{
    // Similar to the above, 'changes' is passed for potential use by recipient.
    RoomIdSet ids = m_map.lookingForRooms(targetCoord);
    for (RoomId id : ids) {
        if (auto rh = m_map.findRoomHandle(id)) {
            // Assuming PathProcessor::receiveRoom now takes ChangeList
            recipient.receiveRoom(rh, changes);
        }
    }
}

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
    // Apply mandatory updates to the current room based on the last known game event.
    changes.add(Change{room_change_types::Update{id, m_lastEvent.deref(), UpdateTypeEnum::Force}});
    updateMostLikelyRoom(m_lastEvent, changes, true);
    if (!changes.empty()) {
        scheduleAction(changes);
    }
}

void PathMachine::slot_releaseAllPaths()
{
    ChangeList localChanges; // Create a local ChangeList
    auto &paths = deref(m_paths);
    for (auto &path : paths) {
        path->deny(localChanges); // Updated call
    }
    paths.clear();

    // If any changes were generated (e.g., rooms removed), schedule them.
    if (!localChanges.empty()) {
        scheduleAction(localChanges);
    }

    m_state = PathStateEnum::SYNCING;

    // REVISIT: should these be cleared, too?
    if ((false)) {
        m_pathRoot.reset();
        m_mostLikelyRoom.reset();
    }
}

// START OF NEW HELPER METHOD IMPLEMENTATIONS

void PathMachine::helperUpdateServerId(const ParseEvent &event, const RoomHandle &here, ChangeList &changes, std::unordered_set<ServerRoomId> &addedServerIds)
{
    if (event.hasServerId()) {
        const auto oldId = here.getServerId();
        const auto newId = event.getServerId();
        if (oldId == INVALID_SERVER_ROOMID && newId != INVALID_SERVER_ROOMID) {
            // Current room doesn't have a server ID, but the event provides one. Set it.
            changes.add(Change{room_change_types::SetServerId{here.getId(), newId}});
            addedServerIds.emplace(newId); // Track that this ID was added in this run
            qInfo() << "Set server id" << newId.asUint32();
        }
    }
}

void PathMachine::helperProcessExitsFromServerIds(const ParseEvent &event, const RoomHandle &here, ChangeList &changes, bool force, std::unordered_set<ServerRoomId> &addedServerIds)
{
    ExitsFlagsType eventExitsFlags = event.getExitsFlags();
    if (eventExitsFlags.isValid()) {
        for (const ExitDirEnum dir : ALL_EXITS_NESWUD) {
            const auto from = here.getId();
            const auto toServerId = event.getExitIds()[dir];
            const auto &roomExit = here.getExit(dir);
            if (roomExit.exitIsNoMatch()) {
                // Skip processing for exits marked as NO_MATCH on the map.
                continue;
            }
            if (toServerId == INVALID_SERVER_ROOMID) {
                // Event data indicates no exit or a hidden exit in this direction.
                if (roomExit.exitIsExit() && !eventExitsFlags.get(dir).isExit()
                    && !roomExit.doorIsHidden()) {
                    // Map has a visible exit, but event says there isn't one.
                    if (force) {
                        // Forcing update: Remove the exit from the map.
                        changes.add(
                            Change{exit_change_types::NukeExit{from, dir, WaysEnum::OneWay}});
                    } else if (roomExit.exitIsDoor()) {
                        // Not forcing: If it's a door on map, mark it as hidden.
                        changes.add(Change{
                            exit_change_types::SetDoorFlags{FlagChangeEnum::Add,
                                                            from,
                                                            dir,
                                                            DoorFlags{DoorFlagEnum::HIDDEN}}});
                    } else {
                        // Not forcing: If it's a regular exit, mark as NO_MATCH to indicate discrepancy.
                        changes.add(Change{
                            exit_change_types::SetExitFlags{FlagChangeEnum::Add,
                                                            from,
                                                            dir,
                                                            ExitFlags{ExitFlagEnum::NO_MATCH}}});
                    }
                }
                continue;
            }
            // Event provides a server ID for the room in this direction.
            if (const auto there = m_map.findRoomHandle(toServerId)) {
                // A room with this server ID already exists in the map.
                const auto to = there.getId();
                if ((getMapMode() == MapModeEnum::MAP || force) && !roomExit.containsOut(to)) {
                    // In mapping mode or forcing update: if current room's exit doesn't lead to 'there', add the connection.
                    changes.add(Change{exit_change_types::ModifyExitConnection{ChangeTypeEnum::Add,
                                                                               from,
                                                                               dir,
                                                                               to,
                                                                               WaysEnum::OneWay}});
                }
            } else if (roomExit.outIsUnique() && addedServerIds.find(toServerId) == addedServerIds.end()) {
                // Map's exit leads to a unique room that doesn't have a server ID yet,
                // and this server ID from event hasn't been assigned during this update run.
                // Assign the server ID from the event to this adjacent room.
                const auto to = roomExit.outFirst();
                changes.add(Change{room_change_types::SetServerId{to, toServerId}});
                addedServerIds.emplace(toServerId); // Track this assignment for current run.
            }
        }
    }
}

void PathMachine::helperUpdateExitAndDoorFlags(const ParseEvent &event, const RoomHandle &here, ChangeList &changes, bool force)
{
    ExitsFlagsType eventExitsFlags = event.getExitsFlags();
    const ConnectedRoomFlagsType connectedRoomFlags = event.getConnectedRoomFlags(); // Needed for road logic

    if (eventExitsFlags.isValid()) {
        const auto &eventExits = event.getExits();
        for (const ExitDirEnum dir : ALL_EXITS_NESWUD) {
            auto &roomExit = here.getExit(dir); // It's okay if this is const, as we are not modifying roomExit directly but adding Changes

            ExitFlags roomExitFlags = roomExit.getExitFlags();
            roomExitFlags.remove(ExitFlagEnum::UNMAPPED); // Make a copy for comparison
            ExitFlags eventExitFlags = eventExitsFlags.get(dir);

            const DoorFlags roomDoorFlags = roomExit.getDoorFlags();
            const DoorFlags eventDoorFlags = eventExits.at(dir).getDoorFlags();

            if (force) {
                if (roomExit.exitIsRoad() && !eventExitFlags.isRoad()
                    && connectedRoomFlags.isValid() && connectedRoomFlags.hasDirectSunlight(dir)) {
                    eventExitFlags |= ExitFlagEnum::ROAD;
                }
                // Forcing update: Set exit flags directly from event.
                changes.add(Change{exit_change_types::SetExitFlags{FlagChangeEnum::Set,
                                                                   here.getId(),
                                                                   dir,
                                                                   eventExitFlags}});
                // Forcing update: Set door flags directly from event.
                changes.add(Change{exit_change_types::SetDoorFlags{FlagChangeEnum::Set,
                                                                   here.getId(),
                                                                   dir,
                                                                   eventDoorFlags}});
            } else {
                // Not forcing: Append flags if different.
                if (roomExit.exitIsNoMatch() || !eventExitsFlags.get(dir).isExit()) {
                    // Skip if map exit is NO_MATCH or event says no exit (already handled for 'force' case).
                    continue;
                }
                if (eventExitFlags ^ roomExitFlags) { // If event flags differ from map flags.
                    // Add differing exit flags from event to the map's exit.
                    changes.add(Change{exit_change_types::SetExitFlags{FlagChangeEnum::Add,
                                                                       here.getId(),
                                                                       dir,
                                                                       eventExitFlags}});
                }
                if (eventDoorFlags ^ roomDoorFlags) { // If event door flags differ.
                    // Add differing door flags from event to the map's exit.
                    changes.add(Change{exit_change_types::SetDoorFlags{FlagChangeEnum::Add,
                                                                       here.getId(),
                                                                       dir,
                                                                       eventDoorFlags}});
                }
            }

            const auto &doorName = eventExits.at(dir).getDoorName();
            if (eventDoorFlags.isHidden() && !doorName.isEmpty()
                && roomExit.getDoorName() != doorName) {
                // If event specifies a door name for a hidden door and it's new/different.
                changes.add(Change{exit_change_types::SetDoorName{here.getId(), dir, doorName}});
            }
        }
    }
}

void PathMachine::helperUpdateRoomLight(const ParseEvent &event, const RoomHandle &here, ChangeList &changes)
{
    const PromptFlagsType pFlags = event.getPromptFlags();
    const ConnectedRoomFlagsType connectedRoomFlags = event.getConnectedRoomFlags(); // For context

    if (pFlags.isValid()) {
        const RoomSundeathEnum sunType = here.getSundeathType();
        if (pFlags.isLit() && sunType == RoomSundeathEnum::NO_SUNDEATH
            && here.getLightType() != RoomLightEnum::LIT) {
            // Event says room is lit, map doesn't, and it's not a sundeath room: update map to lit.
            changes.add(Change{room_change_types::ModifyRoomFlags{here.getId(),
                                                                  RoomLightEnum::LIT,
                                                                  FlagModifyModeEnum::ASSIGN}});
        } else if (pFlags.isDark() && sunType == RoomSundeathEnum::NO_SUNDEATH
                   && here.getLightType() == RoomLightEnum::UNDEFINED
                   && (connectedRoomFlags.isValid() && connectedRoomFlags.hasAnyDirectSunlight())) {
            // Event says room is dark, map light is undefined, not sundeath, but has sunlight access: update map to dark.
            // REVISIT: Can be temporarily dark due to night time or magical darkness
            changes.add(Change{room_change_types::ModifyRoomFlags{here.getId(),
                                                                  RoomLightEnum::DARK,
                                                                  FlagModifyModeEnum::ASSIGN}});
        }
    }
}

void PathMachine::helperUpdateAdjacentRoomSundeath(const ParseEvent &event, const RoomHandle &here, ChangeList &changes)
{
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
                    // Adjacent room is exposed to direct sunlight from this exit, mark it as sundeath.
                    changes.add(
                        Change{room_change_types::ModifyRoomFlags{to,
                                                                  RoomSundeathEnum::SUNDEATH,
                                                                  FlagModifyModeEnum::ASSIGN}});
                } else if (crf.isTrollMode() && crf.hasNoDirectSunlight(dir)
                           && sunType != RoomSundeathEnum::NO_SUNDEATH) {
                    // In troll mode, if adjacent room is not exposed to direct sunlight, mark it as no_sundeath.
                    changes.add(
                        Change{room_change_types::ModifyRoomFlags{to,
                                                                  RoomSundeathEnum::NO_SUNDEATH,
                                                                  FlagModifyModeEnum::ASSIGN}});
                }
            }
        }
    }
}

// END OF NEW HELPER METHOD IMPLEMENTATIONS

void PathMachine::handleParseEvent(const SigParseEvent &sigParseEvent)
{
    if (m_lastEvent != sigParseEvent.requireValid()) {
        m_lastEvent = sigParseEvent;
    }

    m_lastEvent.requireValid();

    m_signaler.clearPendingStatesForCycle(); // Added call

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

void PathMachine::tryExits(const RoomHandle &room,
                           PathProcessor &recipient, // Changed from RoomRecipient
                           const ParseEvent &event,
                           const bool out,
                           ChangeList &changes) // Added ChangeList
{
    if (!room.exists()) {
        // most likely room doesn't exist
        return;
    }

    const CommandEnum move = event.getMoveType();
    if (isDirection7(move)) {
        const auto &possible = room.getExit(getDirection(move));
        tryExit(possible, recipient, out, changes); // Pass changes
    } else {
        // Only check the current room for LOOK
        findAndReceiveRooms(room.getId(), recipient, changes); // Use helper

        if (move >= CommandEnum::FLEE) {
            // Only try all possible exits for commands FLEE, SCOUT, and NONE
            for (const auto &possible : room.getExits()) {
                tryExit(possible, recipient, out, changes); // Pass changes
            }
        }
    }
}

void PathMachine::tryExit(const RawExit &possible, PathProcessor &recipient, bool out, ChangeList &changes)
{
    for (auto idx : (out ? possible.getOutgoingSet() : possible.getIncomingSet())) {
        // Now using the new helper function
        findAndReceiveRooms(idx, recipient, changes);
    }
}

void PathMachine::tryCoordinate(const RoomHandle &room,
                                PathProcessor &recipient, // Changed from RoomRecipient
                                const ParseEvent &event,
                                ChangeList &changes) // Added ChangeList
{
    if (!room.exists()) {
        // most likely room doesn't exist
        return;
    }

    const CommandEnum moveCode = event.getMoveType();
    if (moveCode < CommandEnum::FLEE) {
        // LOOK, UNKNOWN will have an empty offset
        auto offset = ::exitDir(getDirection(moveCode));
        const Coordinate c = room.getPosition() + offset;
        findAndReceiveRooms(c, recipient, changes); // Use helper

    } else {
        const Coordinate roomPos = room.getPosition();
        // REVISIT: Should this enumerate 6 or 7 values?
        // NOTE: This previously enumerated 8 values instead of 7,
        // which meant it was asking for exitDir(ExitDirEnum::NONE),
        // even though both ExitDirEnum::UNKNOWN and ExitDirEnum::NONE
        // both have Coordinate(0, 0, 0).
        for (const ExitDirEnum dir : ALL_EXITS7) {
            findAndReceiveRooms(roomPos + ::exitDir(dir), recipient, changes); // Use helper
        }
    }
}

void PathMachine::approved(const SigParseEvent &sigParseEvent, ChangeList &changes)
{
    ParseEvent &event = sigParseEvent.deref();

    RoomHandle perhaps;
    Approved appr(m_map, sigParseEvent, m_params.matchingTolerance); // Stack allocation

    if (event.hasServerId()) {
        perhaps = m_map.findRoomHandle(event.getServerId());
        if (perhaps.exists()) {
            appr.receiveRoom(perhaps, changes); // Use .
        }
        perhaps = appr.oneMatch(); // Use .
    }

    // This code path only happens for historic maps and mazes where no server id is present
    if (!perhaps) {
        appr.releaseMatch(changes); // Use .

        tryExits(getMostLikelyRoom(), appr, event, true, changes); // Pass by reference
        perhaps = appr.oneMatch(); // Use .

        if (!perhaps) {
            // try to match by reverse exit
            appr.releaseMatch(changes); // Use .
            tryExits(getMostLikelyRoom(), appr, event, false, changes); // Pass by reference
            perhaps = appr.oneMatch(); // Use .
            if (!perhaps) {
                // try to match by coordinate
                appr.releaseMatch(changes); // Use .
                tryCoordinate(getMostLikelyRoom(), appr, event, changes); // Pass by reference
                perhaps = appr.oneMatch(); // Use .
                if (!perhaps) {
                    // try to match by coordinate one step below expected
                    appr.releaseMatch(changes); // Use .
                    // FIXME: need stronger type checking here.

                    const auto cmd = event.getMoveType();
                    // NOTE: This allows ExitDirEnum::UNKNOWN,
                    // which means the coordinate can be Coordinate(0,0,0).
                    const Coordinate &eDir = ::exitDir(getDirection(cmd));

                    // CAUTION: This test seems to mean it wants only NESW,
                    // but it would also accept ExitDirEnum::UNKNOWN,
                    // which in the context of this function would mean "no move."
                    if (eDir.z == 0) {
                        // REVISIT: if we can be sure that this function does not modify the
                        // mostLikelyRoom, then we should retrieve it above, and directly ask
                        // for its position here instead of using this fragile interface.
                        if (const auto &pos = tryGetMostLikelyRoomPosition()) {
                            Coordinate c = pos.value() + eDir;
                            --c.z;
                            RoomIdSet ids_c1 = m_map.lookingForRooms(c);
                            for (RoomId id : ids_c1) {
                                if (auto rh = m_map.findRoomHandle(id)) {
                                    appr.receiveRoom(rh, changes); // Use .
                                }
                            }
                            perhaps = appr.oneMatch(); // Use .

                            if (!perhaps) {
                                // try to match by coordinate one step above expected
                                appr.releaseMatch(changes); // Use .
                                c.z += 2;
                                RoomIdSet ids_c2 = m_map.lookingForRooms(c);
                                for (RoomId id : ids_c2) {
                                    if (auto rh = m_map.findRoomHandle(id)) {
                                        appr.receiveRoom(rh, changes); // Use .
                                    }
                                }
                                perhaps = appr.oneMatch(); // Use .
                            }
                        }
                    }
                }
            }
        }
    }

    if (!perhaps) {
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
        // Using nullptr for PathProcessor* when no specific locker is involved.
        deref(m_paths).push_front(Path::alloc(pathRoot, nullptr, m_signaler, std::nullopt)); // Pass m_signaler by reference
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
                // Player moved: establish a one-way exit from the previous room to the new 'perhaps' room.
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

    if (perhaps.exists()) {
        if (perhaps.isTemporary()) {
            // The matched room was temporary; make it permanent as it's now confirmed.
            changes.add(Change{room_change_types::MakePermanent{perhaps.getId()}});
        }
        // If 'appr.needsUpdate()' was true, that change is added by the Update below
    }
    // If moreThanOne was true, Approved::virt_receiveRoom should have added temporary rooms
    // to 'changes' for removal already.

    if (appr.needsUpdate()) { // Use .
        // The 'Approved' strategy determined the room needs an update based on event details.
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
    const auto here = getMostLikelyRoom(); // Assuming this is valid if function is called

    if (!here.exists()) { // Guard against invalid room handle
        return;
    }

    // This set tracks ServerRoomIds that are assigned to rooms *during this specific run*
    // of updateMostLikelyRoom. This prevents issues where, for example, a room's server ID
    // is set based on the event, and then an exit pointing to it via its *old* (or lack of)
    // server ID tries to also set its server ID again.
    std::unordered_set<ServerRoomId> addedServerIdsForThisRun;

    helperUpdateServerId(event, here, changes, addedServerIdsForThisRun);
    helperProcessExitsFromServerIds(event, here, changes, force, addedServerIdsForThisRun);
    helperUpdateExitAndDoorFlags(event, here, changes, force);
    helperUpdateRoomLight(event, here, changes);
    helperUpdateAdjacentRoomSundeath(event, here, changes);
}

void PathMachine::syncing(const SigParseEvent &sigParseEvent, ChangeList &changes)
{
    auto &params = m_params; // This is m_params from PathMachine
    ParseEvent &event = sigParseEvent.deref();
    {
        Syncing sync_strat(m_params, m_paths, m_signaler); // Stack allocation
        if (event.hasServerId() || event.getNumSkipped() <= m_params.maxSkipped) {
            RoomIdSet ids = m_map.lookingForRooms(sigParseEvent);
            for (RoomId id : ids) {
                if (auto rh = m_map.findRoomHandle(id)) {
                    sync_strat.receiveRoom(rh, changes); // Use .
                }
            }
        }
        m_paths = sync_strat.evaluate(); // Use .
        sync_strat.finalizePaths(changes); // Use .
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
        Crossover exp_strat(m_map, m_paths, dir, m_params); // Stack allocation
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
        // Look for appropriate rooms (including those we just created)
        RoomIdSet ids = m_map.lookingForRooms(sigParseEvent);
        for (RoomId id : ids) {
            if (auto rh = m_map.findRoomHandle(id)) {
                exp_strat.receiveRoom(rh, changes); // Use .
            }
        }
        m_paths = exp_strat.evaluate(changes); // Use .
    } else {
        OneByOne oo_strat(sigParseEvent, m_params, m_signaler); // Stack allocation
        {
            for (const auto &path : deref(m_paths)) {
                const auto &working = path->getRoom();
                oo_strat.addPath(path); // Use .
                tryExits(working, oo_strat, event, true, changes);  // Pass by reference
                tryExits(working, oo_strat, event, false, changes); // Pass by reference
                tryCoordinate(working, oo_strat, event, changes);   // Pass by reference
            }
        }
        m_paths = oo_strat.evaluate(changes); // Use .
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
