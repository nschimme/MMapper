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
#include "syncing.h"

#include <cassert>
#include <memory>
#include <optional>
#include <unordered_set>

PathMachine::PathMachine(MapFrontend &map, QObject *const parent)
    : QObject(parent)
    , m_map{map}
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
    ChangeList changes; // Create ChangeList
    auto &paths = deref(m_paths);
    for (auto &path : paths) {
        path->deny(changes); // Pass ChangeList to deny
    }
    paths.clear();

    if (!changes.empty()) { // Schedule if changes were made
        scheduleAction(changes);
    }

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

void PathMachine::tryExits(const RoomHandle &room,
                           PathProcessor &processor,
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
        tryExit(possible, processor, out);
    } else {
        // Only check the current room for LOOK
        RoomIdSet ids = m_map.lookingForRooms(room.getId());
        for (RoomId id : ids) {
            if (auto rh = m_map.findRoomHandle(id)) {
                processor.processRoom(rh, event);
            }
        }
        if (move >= CommandEnum::FLEE) {
            // Only try all possible exits for commands FLEE, SCOUT, and NONE
            for (const auto &possible : room.getExits()) {
                tryExit(possible, processor, out);
            }
        }
    }
}

void PathMachine::tryExit(const RawExit &possible, PathProcessor &processor, const bool out)
{
    // Note: `event` is not directly available here. Using m_lastEvent as a workaround.
    // This should be reviewed if it causes issues, as m_lastEvent might not always be the relevant event for this specific exit processing.
    ParseEvent &eventForProcessing = m_lastEvent.deref(); // Assuming m_lastEvent is valid here.
    // A better solution might involve passing the relevant ParseEvent down from tryExits,
    // or refactoring how PathProcessor is used if different events are needed.

    for (auto idx : (out ? possible.getOutgoingSet() : possible.getIncomingSet())) {
        RoomIdSet ids = m_map.lookingForRooms(idx);
        for (RoomId id : ids) {
            if (auto rh = m_map.findRoomHandle(id)) {
                processor.processRoom(rh, eventForProcessing);
            }
        }
    }
}

void PathMachine::tryCoordinate(const RoomHandle &room,
                                PathProcessor &processor,
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
        RoomIdSet ids_c = m_map.lookingForRooms(c);
        for (RoomId id : ids_c) {
            if (auto rh = m_map.findRoomHandle(id)) {
                processor.processRoom(rh, event);
            }
        }

    } else {
        const Coordinate roomPos = room.getPosition();
        // REVISIT: Should this enumerate 6 or 7 values?
        // NOTE: This previously enumerated 8 values instead of 7,
        // which meant it was asking for exitDir(ExitDirEnum::NONE),
        // even though both ExitDirEnum::UNKNOWN and ExitDirEnum::NONE
        // both have Coordinate(0, 0, 0).
        for (const ExitDirEnum dir : ALL_EXITS7) {
            RoomIdSet ids_dir = m_map.lookingForRooms(roomPos + ::exitDir(dir));
            for (RoomId id : ids_dir) {
                if (auto rh = m_map.findRoomHandle(id)) {
                    processor.processRoom(rh, event);
                }
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
            // appr.receiveRoom(perhaps); // Old call
            appr.processRoom(perhaps, event); // New call
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
                            RoomIdSet ids_c1 = m_map.lookingForRooms(c);
                            for (RoomId id : ids_c1) {
                                if (auto rh = m_map.findRoomHandle(id)) {
                                    // appr.receiveRoom(rh); // Old call
                                    appr.processRoom(rh, event); // New call
                                }
                            }
                            perhaps = appr.oneMatch();

                            if (!perhaps) {
                                // try to match by coordinate one step above expected
                                appr.releaseMatch();
                                c.z += 2;
                                RoomIdSet ids_c2 = m_map.lookingForRooms(c);
                                for (RoomId id : ids_c2) {
                                    if (auto rh = m_map.findRoomHandle(id)) {
                                        // appr.receiveRoom(rh); // Old call
                                        appr.processRoom(rh, event); // New call
                                    }
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

        /* The Path::alloc signature has changed.
         * Old: Path::alloc(pathRoot, nullptr, &m_signaler, std::nullopt)
         * New: Path::alloc(PathMachine&, const RoomHandle&, std::optional<ExitDirEnum>)
         * The nullptr was for the locker, and &m_signaler for the signaler. Both removed.
         * PathMachine itself is now passed.
         */
        deref(m_paths).push_front(Path::alloc(*this, pathRoot, std::nullopt));
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
        // Syncing constructor changed: Syncing(PathMachine&, PathParameters&, std::shared_ptr<PathList>)
        Syncing sync{*this, params, m_paths};
        if (event.hasServerId() || event.getNumSkipped() <= params.maxSkipped) {
            RoomIdSet ids = m_map.lookingForRooms(sigParseEvent);
            for (RoomId id : ids) {
                if (auto rh = m_map.findRoomHandle(id)) {
                    // sync.receiveRoom(rh); // Old call
                    sync.processRoom(rh, event); // New call
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
                // exp.receiveRoom(rh); // Old call
                exp.processRoom(rh, event); // New call
            }
        }
        m_paths = exp.evaluate();
    } else {
        // OneByOne constructor changed: OneByOne(const SigParseEvent&, PathParameters&)
        OneByOne oneByOne{sigParseEvent, params};
        {
            // auto &tmp = oneByOne; // 'tmp' is 'oneByOne'
            for (const auto &path : deref(m_paths)) {
                const auto &working = path->getRoom();
                oneByOne.addPath(path); // Use oneByOne directly
                tryExits(working, oneByOne, event, true);
                tryExits(working, oneByOne, event, false);
                tryCoordinate(working, oneByOne, event);
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

size_t PathMachine::getNumLockers(RoomId room) const
{
    const auto it = m_roomLockers.find(room);
    if (it != m_roomLockers.end()) {
        return it->second.size();
    }
    return 0;
}

// --- Methods moved from RoomSignalHandler ---

void PathMachine::holdRoom(RoomId room, PathProcessor *processor)
{
    // REVISIT: why do we allow processor to be null? (Original comment from RoomSignalHandler)
    // If PathProcessor is an interface, a null processor might indicate no specific processing needed,
    // but still needs to be tracked for ownership/hold counting.
    m_roomOwners.insert(room);
    if (m_roomLockers[room].empty()) {
        m_roomHoldCount[room] = 0;
    }
    m_roomLockers[room].insert(processor); // processor can be nullptr
    ++m_roomHoldCount[room];
}

void PathMachine::releaseRoom(RoomId room, ChangeList &changes)
{
    assert(m_roomHoldCount.count(room) && m_roomHoldCount[room] > 0);
    if (--m_roomHoldCount[room] == 0) {
        if (m_roomOwners.contains(room)) {
            // New logic to remove room if it's temporary
            if (auto rh = m_map.findRoomHandle(room)) {
                if (rh.isTemporary()) {
                    changes.add(Change{room_change_types::RemoveRoom{room}});
                }
            }
            // Original loop that called m_map.releaseRoom for each recipient is removed.
        } else {
            // This case should ideally not be reached if holdRoom and releaseRoom are used correctly.
            // If it is, it implies a mismatch in hold/release calls or state corruption.
            assert(false && "Released a room that was not properly owned or already fully released.");
        }

        m_roomLockers.erase(room);
        m_roomOwners.erase(room); // Erase from owners only when hold count is zero and after checking temporary status.
        m_roomHoldCount.erase(room); // Clean up the hold count map entry
    }
}

void PathMachine::keepRoom(RoomId room,
                             ExitDirEnum dir,
                             RoomId fromId,
                             ChangeList &changes)
{
    assert(m_roomHoldCount.count(room) && m_roomHoldCount[room] != 0);
    assert(m_roomOwners.contains(room));

    // New logic to make room permanent if it's temporary
    if (auto rh = m_map.findRoomHandle(room)) {
        if (rh.isTemporary()) {
            changes.add(Change{room_change_types::MakePermanent{room}});
        }
    }

    static_assert(static_cast<uint32_t>(ExitDirEnum::UNKNOWN) + 1 == NUM_EXITS);
    if (isNESWUD(dir) || dir == ExitDirEnum::UNKNOWN) { // UNKNOWN is used for e.g. new maze rooms
        changes.add(exit_change_types::ModifyExitConnection{ChangeTypeEnum::Add,
                                                            fromId,
                                                            dir,
                                                            room,
                                                            WaysEnum::OneWay});
    }

    // Original logic for managing lockers, but without calling m_map.keepRoom
    // The problem statement implies PathProcessor replaces RoomRecipient for m_roomLockers.
    // If there was a PathProcessor associated with keeping this room (e.g. the one that discovered it),
    // it might have been stored in m_roomLockers. However, the original RoomSignalHandler::keep
    // just picked one locker arbitrarily to call m_map.keepRoom with.
    // Since we are not calling an equivalent of m_map.keepRoom with a specific processor,
    // and PathProcessor::processRoom is for a different purpose,
    // the direct manipulation of m_roomLockers here (like removing a specific processor)
    // might not be needed unless a PathProcessor instance itself needs to be notified,
    // which is not part of the current interface.
    // The key is that the room is "kept" by virtue of becoming permanent and/or having an exit linked to it.
    // The hold on the room is then released.

    // If a specific processor was involved in 'keeping' and needed to be removed from lockers:
    // if (!m_roomLockers[room].empty()) {
    //     PathProcessor *processor = *(m_roomLockers[room].begin()); // Example: get one processor
    //     if (processor) {
    //         // If PathProcessor had a method like `keptRoom(room)` it could be called here.
    //     }
    //     m_roomLockers[room].erase(processor); // This would be if one specific lock is removed by 'keep'
    // }
    // However, the original logic released *one* locker, then called release(room).
    // The current task is to adapt RoomSignalHandler, which had a slightly ambiguous locker removal.
    // The most important part is calling releaseRoom at the end.

    releaseRoom(room, changes); // This will decrement holdCount and might trigger actual release logic
}
