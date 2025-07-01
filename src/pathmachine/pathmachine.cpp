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

class PathProcessor;

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
    if (id == INVALID_ROOMID) {
        MMLOG() << "in " << __FUNCTION__ << " detected invalid room.";
    }
    const auto &room = m_map.findRoomHandle(id);
    if (!room) {
        slot_releaseAllPaths();
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
    if (room.isTemporary()) {
        changes.add(Change{room_change_types::MakePermanent{id}});
    }
    updateMostLikelyRoom(m_lastEvent, changes, true);
    if (!changes.empty()) {
        scheduleAction(changes);
    }
}

void PathMachine::slot_releaseAllPaths()
{
    auto &paths = deref(m_paths);
    for (auto &path : paths) {
        path->deny(m_masterChanges); // Pass m_masterChanges
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
    // Clear batched changes and pending path segments at the beginning of each event cycle
    m_tempRoomCreationChanges = ChangeList{};
    m_masterChanges = ChangeList{};
    m_pendingPathSegments.clear();

    if (m_lastEvent != sigParseEvent.requireValid()) {
        m_lastEvent = sigParseEvent;
    }
    m_lastEvent.requireValid();

    // --- Initial Logic Block ---
    if (m_map.getCurrentMap().empty()) {
        Coordinate c(0, 0, 0);
        // This initial room creation goes to master changes.
        m_masterChanges.add(room_change_types::AddRoom2{c, sigParseEvent.deref()});

        // Apply this immediately as it's a bootstrap case. This is Transaction 1.
        if (getMapMode() != MapModeEnum::OFFLINE && !m_masterChanges.empty()) {
            m_map.applyChanges(m_masterChanges);
        }
        // m_masterChanges is now applied. Clear it before forcePositionChange potentially adds to it.
        m_masterChanges = ChangeList{};

        RoomHandle bootstrappedRoomHandle = m_map.findRoomHandle(c);
        if (bootstrappedRoomHandle) {
            // forcePositionChange will add to m_masterChanges via its own scheduleAction.
            forcePositionChange(bootstrappedRoomHandle.getId(), false);

            // Apply these secondary changes from forcePositionChange immediately as well. This is Transaction 2.
            if (getMapMode() != MapModeEnum::OFFLINE && !m_masterChanges.empty()) {
                 m_map.applyChanges(m_masterChanges);
                 m_masterChanges = ChangeList{}; // Clear after applying.
            }
        }
        return; // Bootstrap case is complete.
    }

    // This local ChangeList is used by the state-specific logic (approved, experimenting, syncing)
    // and updateMostLikelyRoom to collect changes that are NOT temporary room creations initially.
    ChangeList initial_phase_changes;

    // Main state logic. `experimenting` will be modified in Step 2 to populate
    // m_tempRoomCreationChanges & m_pendingPathSegments.
    // It can also add to `initial_phase_changes` for operations on existing paths.
    switch (m_state) {
    case PathStateEnum::APPROVED:
        approved(sigParseEvent, initial_phase_changes);
        break;
    case PathStateEnum::EXPERIMENTING:
        experimenting(sigParseEvent, initial_phase_changes);
        break;
    case PathStateEnum::SYNCING:
        syncing(sigParseEvent, initial_phase_changes);
        break;
    }

    if (m_state == PathStateEnum::APPROVED && hasMostLikelyRoom()) {
        updateMostLikelyRoom(sigParseEvent, initial_phase_changes, false);
    }

    if (!initial_phase_changes.empty()) {
        scheduleAction(initial_phase_changes); // Adds to m_masterChanges
    }

    // --- Transaction 1: Apply Temporary Room Creations ---
    if (!m_tempRoomCreationChanges.empty()) {
        if (getMapMode() != MapModeEnum::OFFLINE) {
            m_map.applyChanges(m_tempRoomCreationChanges);
        }
        // Temp changes are applied. Don't clear m_tempRoomCreationChanges yet if its contents (like coords)
        // are needed by m_pendingPathSegments logic. The plan is to clear it after its info is used.
    }

    // --- Resolve New RoomIds and Create/Update Path Objects ---
    if (!m_pendingPathSegments.empty()) {
        // auto& current_paths_list = deref(m_paths); // Not directly adding to m_paths here. Path::fork handles linkage.
        for (const auto& segment : m_pendingPathSegments) {
            if (auto parent = segment.parentPath.lock()) {
                RoomHandle newRoomHandle = m_map.findRoomHandle(segment.newRoomCoord);
                if (newRoomHandle) {
                    // Path::fork creates the new path, links it to parent, calls 'hold', etc.
                    // The new path segment is now part of the explorable path structure via its parent.
                    parent->fork(newRoomHandle, segment.newRoomCoord, m_params, segment.viaDir);
                }
            }
            // TODO in Step 2 & 3: Handle cases where parentPath might be null for new root paths,
            // Path::alloc would be used and the new path added to m_paths.
            // For now, PendingPathSegmentContext implies a parent.
        }
        m_pendingPathSegments.clear();
    }
    // Now that coordinates from m_tempRoomCreationChanges have been used to find RoomHandles (if any),
    // and m_pendingPathSegments is processed, clear m_tempRoomCreationChanges.
    m_tempRoomCreationChanges = ChangeList{};

    // --- Path Evaluation Phase ---
    // This list collects changes from approving/denying paths, including MakePermanent for new rooms.
    ChangeList eval_changes;
    evaluatePaths(eval_changes); // Operates on m_paths, which might now include newly forked paths to just-created rooms.
    if (!eval_changes.empty()) {
        scheduleAction(eval_changes); // Adds to m_masterChanges
    }

    // --- Transaction 2: Apply Master Changes ---
    // This includes changes from initial_phase_changes AND eval_changes.
    if (!m_masterChanges.empty()) {
        if (getMapMode() != MapModeEnum::OFFLINE) {
            m_map.applyChanges(m_masterChanges);
        }
        m_masterChanges = ChangeList{};
    }

    // --- Final Signal ---
    if (m_state != PathStateEnum::SYNCING) {
        if (const auto room = getMostLikelyRoom()) {
            emit sig_playerMoved(room.getId());
        }
    }
}

void PathMachine::tryExits(const RoomHandle &room,
                           PathProcessor &recipient,
                           const ParseEvent &event,
                           const bool out)
{
    if (!room.exists()) {
        // most likely room doesn't exist
        return;
    }

    const CommandEnum move = event.getMoveType();
    if (isDirection7(move)) {
        const auto &possible = room.getExit(getDirection(move));
        tryExit(possible, recipient, out);
    } else {
        // Only check the current room for LOOK
        if (auto rh = m_map.findRoomHandle(room.getId())) {
            recipient.receiveRoom(rh);
        }
        if (move >= CommandEnum::FLEE) {
            // Only try all possible exits for commands FLEE, SCOUT, and NONE
            for (const auto &possible : room.getExits()) {
                tryExit(possible, recipient, out);
            }
        }
    }
}

void PathMachine::tryExit(const RawExit &possible, PathProcessor &recipient, const bool out)
{
    for (auto idx : (out ? possible.getOutgoingSet() : possible.getIncomingSet())) {
        if (auto rh = m_map.findRoomHandle(idx)) {
            recipient.receiveRoom(rh);
        }
    }
}

void PathMachine::tryCoordinate(const RoomHandle &room,
                                PathProcessor &recipient,
                                const ParseEvent &event)
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
        if (auto rh = m_map.findRoomHandle(c)) {
            recipient.receiveRoom(rh);
        }

    } else {
        const Coordinate roomPos = room.getPosition();
        // REVISIT: Should this enumerate 6 or 7 values?
        // NOTE: This previously enumerated 8 values instead of 7,
        // which meant it was asking for exitDir(ExitDirEnum::NONE),
        // even though both ExitDirEnum::UNKNOWN and ExitDirEnum::NONE
        // both have Coordinate(0, 0, 0).
        for (const ExitDirEnum dir : ALL_EXITS7) {
            if (auto rh = m_map.findRoomHandle(roomPos + ::exitDir(dir))) {
                recipient.receiveRoom(rh);
            }
        }
    }
}

void PathMachine::approved(const SigParseEvent &sigParseEvent, ChangeList &changes)
{
    ParseEvent &event = sigParseEvent.deref();

    RoomHandle perhaps;
    Approved appr{m_map, sigParseEvent, m_params.matchingTolerance};

    if (event.hasServerId()) {
        perhaps = m_map.findRoomHandle(event.getServerId());
        if (perhaps.exists()) {
            appr.receiveRoom(perhaps);
        }
        perhaps = appr.oneMatch();
    }

    // This code path only happens for historic maps and mazes where no server id is present
    if (!perhaps) {
        appr.releaseMatch();

        tryExits(getMostLikelyRoom(), appr, event, true);
        perhaps = appr.oneMatch();

        if (!perhaps) {
            // try to match by reverse exit
            appr.releaseMatch();
            tryExits(getMostLikelyRoom(), appr, event, false);
            perhaps = appr.oneMatch();
            if (!perhaps) {
                // try to match by coordinate
                appr.releaseMatch();
                tryCoordinate(getMostLikelyRoom(), appr, event);
                perhaps = appr.oneMatch();
                if (!perhaps) {
                    // try to match by coordinate one step below expected
                    appr.releaseMatch();
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
                            if (auto rh = m_map.findRoomHandle(c)) {
                                appr.receiveRoom(rh);
                            }
                            perhaps = appr.oneMatch();

                            if (!perhaps) {
                                // try to match by coordinate one step above expected
                                appr.releaseMatch();
                                c.z += 2;
                                if (auto rh = m_map.findRoomHandle(c)) {
                                    appr.receiveRoom(rh);
                                }
                                perhaps = appr.oneMatch();
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

        deref(m_paths).push_front(Path::alloc(pathRoot, m_signaler, std::nullopt));
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
            if ((ex.exitIsUnmapped()
                 && (toServerId == room.getServerId() // ServerId must agree
                     || toServerId
                            == INVALID_SERVER_ROOMID) // or when ServerId is missing (cannot see exit)
                 && !ex.containsOut(to))
                || !event.getExitsFlags().isValid()) { // or when in a maze that hides exits
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
        Syncing sync{params, m_paths, m_signaler};
        if (event.getNumSkipped() <= params.maxSkipped) {
            RoomIdSet ids = m_map.lookingForRooms(sigParseEvent);
            if (event.hasServerId()) {
                if (auto rh = m_map.findRoomHandle(event.getServerId())) {
                    ids.insert(rh.getId());
                }
            }
            for (RoomId id : ids) {
                if (auto rh = m_map.findRoomHandle(id)) {
                    sync.receiveRoom(rh);
                }
            }
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
        Crossover exp{m_map, m_paths, dir, params};
        RoomIdSet pathEnds;
        // This part of experimenting is responsible for identifying potential new temporary rooms.
        // It populates m_tempRoomCreationChanges and m_pendingPathSegments.
        // The actual Path objects for these new rooms will be created in handleParseEvent
        // after these temporary rooms are applied and have RoomIds.
        RoomIdSet pathEnds; // To avoid creating multiple rooms from the same path end.
        for (const auto &current_path_ptr : deref(m_paths)) { // Iterate over existing paths
            if (!current_path_ptr) continue; // Should not happen with shared_ptr in list
            const RoomHandle &working_room = current_path_ptr->getRoom();

            if (!working_room.exists()) continue;

            const RoomId working_room_id = working_room.getId();
            if (!pathEnds.contains(working_room_id)) {
                if (getMapMode() == MapModeEnum::MAP) {
                    Coordinate new_room_coord = working_room.getPosition() + move;
                    m_tempRoomCreationChanges.add(room_change_types::AddRoom2{new_room_coord, sigParseEvent.deref()});

                    // Store context for creating the Path object later in handleParseEvent
                    m_pendingPathSegments.push_back(PendingPathSegmentContext{
                        new_room_coord,
                        current_path_ptr, // Store weak_ptr to the parent path
                        dir,        // The direction from parent to this new room
                        sigParseEvent // The event triggering this
                    });
                }
                pathEnds.insert(working_room_id);
            }
        }

        // The Crossover (or OneByOne) strategy will then be run.
        // IMPORTANT: These strategies will operate on the *current* state of m_paths,
        // which contains paths to *existing* rooms. They will not yet see paths
        // for the temporary rooms just added to m_tempRoomCreationChanges.
        // Their `evaluate()` methods might update `m_paths` based on existing rooms.
        // The new paths (from m_pendingPathSegments) are added to m_paths later in handleParseEvent.

        Crossover exp{m_map, m_paths, dir, params}; // Renamed exp_processor to exp
        RoomIdSet ids = m_map.lookingForRooms(sigParseEvent); // Renamed existing_room_ids to ids
        if (event.hasServerId()) {
            if (auto rh = m_map.findRoomHandle(event.getServerId())) {
                ids.insert(rh.getId());
            }
        }
        for (RoomId id : ids) { // Use the renamed 'ids'
            if (auto rh = m_map.findRoomHandle(id)) {
                exp.receiveRoom(rh); // Use the renamed 'exp'
            }
        }
        m_paths = exp.evaluate(); // Use the renamed 'exp'
    } else {
        OneByOne oneByOne{sigParseEvent, params, m_signaler};
        {
            auto &tmp = oneByOne;
            for (const auto &path : deref(m_paths)) {
                const auto &working = path->getRoom();
                tmp.addPath(path);
                tryExits(working, tmp, event, true);
                tryExits(working, tmp, event, false);
                tryCoordinate(working, tmp, event);
            }
        }
        m_paths = oneByOne.evaluate();
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
        // Append to master changes instead of applying directly
        for (const auto& change : action.getChanges()) {
            m_masterChanges.add(change);
        }
    }
}

// Old applyBatchedChanges method is now removed. Its logic is integrated into handleParseEvent.

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
