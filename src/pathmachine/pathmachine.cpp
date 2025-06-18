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

#include "../global/Diff.h"
#include "../global/TextUtils.h"
#include "../global/logging.h"
#include "../map/roomid.h" // For RoomDesc

#include <cassert>
#include <cmath> // For std::isfinite, std::nextafter, std::lround
#include <memory>
#include <optional>
#include <sstream> // For std::ostringstream (used indirectly by MyDiff, good to have)
#include <string_view>
#include <unordered_set>
#include <vector> // For std::vector

class RoomRecipient;

namespace { // anonymous

// REVISIT: This is likely to be a performance problem.
NODISCARD static bool isMostlyTheSame(const RoomDesc &a, const RoomDesc &b, const double cutoff)
{
    assert(std::isfinite(cutoff)
           && utils::isClamped(cutoff, std::nextafter(50.0, 51.0), std::nextafter(100.0, 99.0)));

    if (a.empty() || b.empty()) {
        return false;
    }

    struct NODISCARD MyDiff final : public diff::Diff<StringView>
    {
    public:
        size_t removedBytes = 0;
        size_t addedBytes = 0;
        size_t commonBytes = 0;

    private:
        void virt_report(diff::SideEnum side, const Range &r) final
        {
            switch (side) {
            case diff::SideEnum::A:
                for (auto x : r) {
                    removedBytes += x.size();
                }
                break;
            case diff::SideEnum::B:
                for (auto x : r) {
                    addedBytes += x.size();
                }
                break;
            case diff::SideEnum::Common:
                for (auto x : r) {
                    commonBytes += x.size();
                }
                break;
            }
        }
    };

    const std::vector<StringView> aWords = StringView{a.getStdStringViewUtf8()}.getWords();
    const std::vector<StringView> bWords = StringView{b.getStdStringViewUtf8()}.getWords();

    MyDiff diff;
    diff.compare(MyDiff::Range::fromVector(aWords), MyDiff::Range::fromVector(bWords));

    const auto max_change = std::max(diff.removedBytes, diff.addedBytes);
    const auto min_change = std::min(diff.removedBytes, diff.addedBytes);
    const auto total = min_change + diff.commonBytes;

    if (total < 20 || max_change >= total) {
        return false;
    }

    const auto ratio = static_cast<double>(total - max_change) / static_cast<double>(total);
    const auto percent = ratio * 100.0;
    const auto rounded = static_cast<double>(std::lround(percent * 10.0)) * 0.1;

    // REVISIT: MMLOG is specific to mmapper. If this function is truly generic,
    // consider making logging optional or passed in.
    MMLOG() << "[PathMachine] Score: " << rounded << "% word match (isMostlyTheSame)";
    return rounded >= cutoff;
}

} // namespace

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

void PathMachine::tryExits(const RoomHandle &room,
                           RoomRecipient &recipient,
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
        m_map.lookingForRooms(recipient, room.getId());
        if (move >= CommandEnum::FLEE) {
            // Only try all possible exits for commands FLEE, SCOUT, and NONE
            for (const auto &possible : room.getExits()) {
                tryExit(possible, recipient, out);
            }
        }
    }
}

void PathMachine::tryExit(const RawExit &possible, RoomRecipient &recipient, const bool out)
{
    for (auto idx : (out ? possible.getOutgoingSet() : possible.getIncomingSet())) {
        m_map.lookingForRooms(recipient, idx);
    }
}

void PathMachine::tryCoordinate(const RoomHandle &room,
                                RoomRecipient &recipient,
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
        m_map.lookingForRooms(recipient, c);

    } else {
        const Coordinate roomPos = room.getPosition();
        // REVISIT: Should this enumerate 6 or 7 values?
        // NOTE: This previously enumerated 8 values instead of 7,
        // which meant it was asking for exitDir(ExitDirEnum::NONE),
        // even though both ExitDirEnum::UNKNOWN and ExitDirEnum::NONE
        // both have Coordinate(0, 0, 0).
        for (const ExitDirEnum dir : ALL_EXITS7) {
            m_map.lookingForRooms(recipient, roomPos + ::exitDir(dir));
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
                            m_map.lookingForRooms(appr, c);
                            perhaps = appr.oneMatch();

                            if (!perhaps) {
                                // try to match by coordinate one step above expected
                                appr.releaseMatch();
                                c.z += 2;
                                m_map.lookingForRooms(appr, c);
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

    // --- Start of integrated maybeUpdate logic ---
    // This logic is now correctly placed inside updateMostLikelyRoom.
    // 'event' and 'here' are parameters or local variables of updateMostLikelyRoom.

    const auto &evName = event.getRoomName();
    const auto &evDesc = event.getRoomDesc();

    if (evName.empty() || evDesc.empty()) {
        return; // Nothing to compare against from the event
    }

    const auto currentName = here.getName();
    const auto currentDesc = here.getDescription();

    if (currentName == evName && currentDesc == evDesc) {
        return; // Already an exact match
    }

    // If current name or description is empty, it might be a new or uninitialized room.
    // Avoid auto-updating based on similarity in such cases.
    // This mirrors the "!expected.isUpToDate() || name.empty() || desc.empty()" check.
    if (currentName.empty() || currentDesc.empty()) {
        return;
    }

    // Helper lambda to add changes for updating room properties
    auto update_field = [&](const char *fieldNameStr, auto fieldProperty) {
        changes.add(Change{room_change_types::ModifyRoomFlags{here.getId(),
                                                              fieldProperty,
                                                              FlagModifyModeEnum::ASSIGN}});
        MMLOG() << "[PathMachine] Auto-updated " << fieldNameStr << " for room "
                << here.getId().asUint32() << " (was "
                << (std::string_view(fieldNameStr) == "name" ? currentName.getStdStringViewUtf8()
                                                             : currentDesc.getStdStringViewUtf8())
                << ", is now "
                << (std::string_view(fieldNameStr) == "name" ? evName.getStdStringViewUtf8()
                                                             : evDesc.getStdStringViewUtf8())
                << ").";
    };

    if (currentName == evName && isMostlyTheSame(currentDesc, evDesc, 80.0)) {
        MMLOG() << "[PathMachine] Exact name match, description is ~80% similar.";
        update_field("description", evDesc);
    } else if (currentDesc == evDesc && currentName != evName) {
        // Ensure name is not already an exact match, which is covered by the initial check.
        // This branch is for when description is exact, but name is different (and not empty).
        MMLOG() << "[PathMachine] Exact description match, name is different.";
        update_field("name", evName);
    } else if (isMostlyTheSame(currentDesc, evDesc, 90.0)) {
        // This condition is less strict for description if name doesn't match.
        // We also want to ensure that we are not downgrading an exact name match to a fuzzy desc match.
        // This means this block should ideally only be hit if currentName != evName.
        if (currentName != evName) {
             MMLOG() << "[PathMachine] Description is ~90% similar (name differs).";
             update_field("description", evDesc);
        }
        // If currentName == evName, the first condition (80% desc similarity) would have caught it.
        // If we reach here and currentName == evName, it means description similarity was < 80%.
    }
    // --- End of integrated maybeUpdate logic ---
}

void PathMachine::syncing(const SigParseEvent &sigParseEvent, ChangeList &changes)
{
    auto &params = m_params;
    ParseEvent &event = sigParseEvent.deref();
    {
        Syncing sync{params, m_paths, &m_signaler};
        if (event.hasServerId() || event.getNumSkipped() <= params.maxSkipped) {
            m_map.lookingForRooms(sync, sigParseEvent);
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
        m_map.lookingForRooms(exp, sigParseEvent);
        m_paths = exp.evaluate();
    } else {
        OneByOne oneByOne{sigParseEvent, params, &m_signaler};
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
