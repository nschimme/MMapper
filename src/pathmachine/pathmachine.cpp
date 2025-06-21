// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Ulf Hermann <ulfonk_mennhar@gmx.de> (Alve)
// Author: Marek Krejza <krejza@gmail.com> (Caligor)
// Author: Nils Schimmelmann <nschimme@gmail.com> (Jahara)

#include "pathmachine.h"
#include "patheventcontext.h" // Add this include

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
    ChangeList changesForForcePosition; // Create a local ChangeList for this operation
    // Create a PathEventContext for this specific operation.
    // m_lastEvent.deref() is used as currentEvent.
    mmapper::PathEventContext context(m_lastEvent.deref(), changesForForcePosition, m_map);

    // Use context.changes.add() for non-lifecycle changes.
    context.changes.add(Change{room_change_types::Update{id, context.currentEvent, UpdateTypeEnum::Force}});
    if (room.isTemporary()) {
        // Use context.addTrackedChange for MakePermanent
        context.addTrackedChange(Change{room_change_types::MakePermanent{id}});
    }

    // updateMostLikelyRoom now takes PathEventContext& and bool force.
    // The 'context' here is local to forcePositionChange.
    updateMostLikelyRoom(context, true);

    if (!context.changes.empty()) {
        scheduleAction(context.changes);
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

    if (m_map.getCurrentMap().empty()) {
        // Bootstrap an empty map with its first room
        Coordinate c(0, 0, 0);
        // Pass sigParseEvent directly here as slot_createRoom expects it.
        // PathEventContext is not relevant for this bootstrap.
        m_map.slot_createRoom(sigParseEvent, c);
        if (auto rh = m_map.findRoomHandle(c)) {
            forcePositionChange(rh.getId(), false);
            return;
        }
    }

    ChangeList changesForThisEvent;
    // It's important that m_lastEvent.deref() is valid and the correct event.
    // The check sigParseEvent.requireValid() and assignment m_lastEvent = sigParseEvent
    // should ensure m_lastEvent is the one to use.
    mmapper::PathEventContext eventContext(m_lastEvent.deref(), changesForThisEvent, m_map);

    switch (m_state) {
    case PathStateEnum::APPROVED:
        approved(eventContext);
        break;
    case PathStateEnum::EXPERIMENTING:
        experimenting(eventContext);
        break;
    case PathStateEnum::SYNCING:
        syncing(eventContext);
        break;
    }

    if (m_state == PathStateEnum::APPROVED && hasMostLikelyRoom()) {
        // updateMostLikelyRoom now takes PathEventContext& and bool force.
        updateMostLikelyRoom(eventContext, false);
    }

    if (!eventContext.changes.empty()) {
        scheduleAction(eventContext.changes);
    }

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

void PathMachine::approved(mmapper::PathEventContext &context)
{
    // ParseEvent &event = sigParseEvent.deref(); becomes:
    ParseEvent &event = context.currentEvent;

    RoomHandle perhaps;
    // Approved constructor now takes PathEventContext& and int
    Approved appr{context, m_params.matchingTolerance};


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

        // The Path::alloc call below previously passed a nullptr as PathProcessor.
        // This was a concern for RoomSignalHandler::keep if it directly used that processor.
        // Refactoring has removed PathProcessor from Path::alloc and direct use in RoomSignalHandler::keep,
        // resolving the original FIXME about a potential null pointer issue.
        deref(m_paths).push_front(Path::alloc(pathRoot, m_signaler, std::nullopt));
        // Call experimenting with the context
        experimenting(context);

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
                // Use context.changes (directly for now, will be context.addTrackedChange)
                context.changes.add(Change{exit_change_types::ModifyExitConnection{ChangeTypeEnum::Add,
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
        // Use context.changes (directly for now, will be context.addTrackedChange)
        // And context.currentEvent instead of sigParseEvent.deref()
        context.changes.add(Change{room_change_types::Update{perhaps.getId(),
                                                             context.currentEvent,
                                                             UpdateTypeEnum::Update}});
    }
}

// void PathMachine::updateMostLikelyRoom(const SigParseEvent &sigParseEvent, ChangeList &changes, bool force)
void PathMachine::updateMostLikelyRoom(mmapper::PathEventContext &context, bool force)
{
    // ParseEvent &event = sigParseEvent.deref(); becomes:
    ParseEvent &event = context.currentEvent;

    // ChangeList &changes parameter is now context.changes

    // guaranteed to succeed, since it's set above.
    const auto here = getMostLikelyRoom();

    // track added ServerIds to prevent multiple allocations
    std::unordered_set<ServerRoomId> addedIds;

    if (event.hasServerId()) {
        const auto oldId = here.getServerId();
        const auto newId = event.getServerId();
        if (oldId == INVALID_SERVER_ROOMID && newId != INVALID_SERVER_ROOMID) {
            // changes.add becomes context.changes.add
            context.changes.add(Change{room_change_types::SetServerId{here.getId(), newId}});
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
                        context.changes.add( // context.changes
                            Change{exit_change_types::NukeExit{from, dir, WaysEnum::OneWay}});
                    } else if (roomExit.exitIsDoor()) {
                        // Map is old and needs hidden flag
                        context.changes.add(Change{ // context.changes
                            exit_change_types::SetDoorFlags{FlagChangeEnum::Add,
                                                            from,
                                                            dir,
                                                            DoorFlags{DoorFlagEnum::HIDDEN}}});
                    } else {
                        // Use NO_MATCH as a hint to the user which exit isn't matching
                        context.changes.add(Change{ // context.changes
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
                    context.changes.add(Change{exit_change_types::ModifyExitConnection{ChangeTypeEnum::Add, // context.changes
                                                                               from,
                                                                               dir,
                                                                               to,
                                                                               WaysEnum::OneWay}});
                }
            } else if (roomExit.outIsUnique() && addedIds.find(toServerId) == addedIds.end()) {
                // Add likely ServerId
                const auto to = roomExit.outFirst();
                context.changes.add(Change{room_change_types::SetServerId{to, toServerId}}); // context.changes
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

                context.changes.add(Change{exit_change_types::SetExitFlags{FlagChangeEnum::Set, // context.changes
                                                                   here.getId(),
                                                                   dir,
                                                                   eventExitFlags}});
                context.changes.add(Change{exit_change_types::SetDoorFlags{FlagChangeEnum::Set, // context.changes
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
                    context.changes.add(Change{exit_change_types::SetExitFlags{FlagChangeEnum::Add, // context.changes
                                                                       here.getId(),
                                                                       dir,
                                                                       eventExitFlags}});
                }
                if (eventDoorFlags ^ roomDoorFlags) {
                    context.changes.add(Change{exit_change_types::SetDoorFlags{FlagChangeEnum::Add, // context.changes
                                                                       here.getId(),
                                                                       dir,
                                                                       eventDoorFlags}});
                }
            }

            const auto &doorName = eventExits.at(dir).getDoorName();
            if (eventDoorFlags.isHidden() && !doorName.isEmpty()
                && roomExit.getDoorName() != doorName) {
                context.changes.add(Change{exit_change_types::SetDoorName{here.getId(), dir, doorName}}); // context.changes
            }
        }
    }

    const PromptFlagsType pFlags = event.getPromptFlags();
    if (pFlags.isValid()) {
        const RoomSundeathEnum sunType = here.getSundeathType();
        if (pFlags.isLit() && sunType == RoomSundeathEnum::NO_SUNDEATH
            && here.getLightType() != RoomLightEnum::LIT) {
            context.changes.add(Change{room_change_types::ModifyRoomFlags{here.getId(), // context.changes
                                                                  RoomLightEnum::LIT,
                                                                  FlagModifyModeEnum::ASSIGN}});

        } else if (pFlags.isDark() && sunType == RoomSundeathEnum::NO_SUNDEATH
                   && here.getLightType() == RoomLightEnum::UNDEFINED
                   && (connectedRoomFlags.isValid() && connectedRoomFlags.hasAnyDirectSunlight())) {
            // REVISIT: Can be temporarily dark due to night time or magical darkness
            context.changes.add(Change{room_change_types::ModifyRoomFlags{here.getId(), // context.changes
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
                    context.changes.add( // context.changes
                        Change{room_change_types::ModifyRoomFlags{to,
                                                                  RoomSundeathEnum::SUNDEATH,
                                                                  FlagModifyModeEnum::ASSIGN}});
                } else if (crf.isTrollMode() && crf.hasNoDirectSunlight(dir)
                           && sunType != RoomSundeathEnum::NO_SUNDEATH) {
                    context.changes.add( // context.changes
                        Change{room_change_types::ModifyRoomFlags{to,
                                                                  RoomSundeathEnum::NO_SUNDEATH,
                                                                  FlagModifyModeEnum::ASSIGN}});
                }
            }
        }
    }
}

void PathMachine::syncing(mmapper::PathEventContext &context)
{
    auto &params = m_params;
    // ParseEvent &event = sigParseEvent.deref(); becomes:
    ParseEvent &event = context.currentEvent;
    {
        // Syncing class constructor will be updated later to accept PathEventContext (or relevant parts)
        // For now, adapt what we can. This may cause a compile error until Syncing is updated.
        // Syncing sync{params, m_paths, m_signaler};
        // Temporary adaptation, assuming Syncing will be refactored in Step 6
        // Syncing likely needs context.currentEvent rather than the whole context.
        // Or PathMachine members like m_paths, m_signaler, which it already has access to if it's a member or helper.
        // The original Syncing constructor takes (PathParameters&, std::shared_ptr<PathList>&, RoomSignalHandler&)
        // So, it doesn't directly use SigParseEvent in its constructor.
        // The lookForRooms call inside uses sigParseEvent.
        Syncing sync{params, m_paths, m_signaler}; // No change to constructor call itself yet.

        if (event.getNumSkipped() <= params.maxSkipped) {
            // RoomIdSet ids = m_map.lookingForRooms(sigParseEvent); becomes:
            RoomIdSet ids = m_map.lookingForRooms(context.currentEvent);
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
    // evaluatePaths(changes); becomes:
    evaluatePaths(context);
}

void PathMachine::experimenting(mmapper::PathEventContext &context)
{
    auto &params = m_params;
    // ParseEvent &event = sigParseEvent.deref(); becomes:
    ParseEvent &event = context.currentEvent;
    const CommandEnum moveCode = event.getMoveType();

    // only create rooms if it has a serverId or no properties are skipped and the direction is NESWUD.
    if (event.canCreateNewRoom() && isDirectionNESWUD(moveCode) && hasMostLikelyRoom()) {
        const auto dir = getDirection(moveCode);
        const Coordinate &move = ::exitDir(dir);
        // Crossover constructor now takes PathEventContext&, shared_ptr<PathList>, ExitDirEnum, PathParameters&
        Crossover exp{context, m_paths, dir, params};
        RoomIdSet pathEnds;
        for (const auto &path : deref(m_paths)) {
            const auto &working = path->getRoom();
            const RoomId workingId = working.getId();
            if (!pathEnds.contains(workingId)) {
                qInfo() << "creating RoomId" << workingId.asUint32();
                if (getMapMode() == MapModeEnum::MAP) {
                    // m_map.slot_createRoom(sigParseEvent, working.getPosition() + move); becomes:
                    m_map.slot_createRoom(context.currentEvent, working.getPosition() + move);
                }
                pathEnds.insert(workingId);
            }
        }
        // Look for appropriate rooms (including those we just created)
        // RoomIdSet ids = m_map.lookingForRooms(sigParseEvent); becomes:
        RoomIdSet ids = m_map.lookingForRooms(context.currentEvent);
        if (event.hasServerId()) {
            if (auto rh = m_map.findRoomHandle(event.getServerId())) {
                ids.insert(rh.getId());
            }
        }
        for (RoomId id : ids) {
            if (auto rh = m_map.findRoomHandle(id)) {
                exp.receiveRoom(rh);
            }
        }
        m_paths = exp.evaluate();
    } else {
        // OneByOne class constructor will be updated later.
        // OneByOne constructor now takes PathEventContext&, PathParameters&, RoomSignalHandler&
        OneByOne oneByOne{context, params, m_signaler};
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

    // evaluatePaths(changes); becomes:
    evaluatePaths(context);
}

void PathMachine::evaluatePaths(mmapper::PathEventContext &context)
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
        // Path::approve now takes PathEventContext&
        deref(path).approve(context);
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
