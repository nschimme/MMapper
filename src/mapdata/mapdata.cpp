// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Ulf Hermann <ulfonk_mennhar@gmx.de> (Alve)
// Author: Marek Krejza <krejza@gmail.com> (Caligor)
// Author: Nils Schimmelmann <nschimme@gmail.com> (Jahara)

#include "mapdata.h"

#include "../display/MapCanvasRoomDrawer.h"
#include "../display/Textures.h"
#include "../global/AnsiOstream.h"
#include "../global/Timer.h"
#include "../global/logging.h"
#include "../global/progresscounter.h"
#include "../global/utils.h"
#include "../map/CommandId.h"
#include "../map/ExitDirection.h"
#include "../map/ExitFieldVariant.h"
#include "../map/RawRoom.h"
#include "../map/RoomRecipient.h"
#include "../map/World.h"
#include "../map/coordinate.h"
#include "../map/exit.h"
#include "../map/infomark.h"
#include "../map/mmapper2room.h"
#include "../map/room.h"
#include "../map/roomid.h"
#include "../map/RawRoom.h" // For change.data in RemoveRoom
#include "../map/exit.h"   // For RawExit in RemoveRoom
#include "../map/AbstractChangeVisitor.h"
#include "../map/ChangeTypes.h"
#include "../map/world_change_types.h" // For XFOREACH_WORLD_CHANGE
#include "../mapfrontend/mapfrontend.h"
#include "../mapstorage/RawMapData.h"
#include "GenericFind.h"
#include "roomfilter.h"
#include "roomselection.h"

#include <algorithm>
#include <cassert>
#include <iomanip>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <optional>
#include <ostream>
#include <set>
#include <sstream>
#include <tuple>
#include <variant> // Added for std::visit and std::variant
#include <vector>

#include <QApplication>
#include <QList>
#include <QString>
#include <string_view> // For std::less<RoomArea>

// Custom std::less specialization for RoomArea
namespace std {
    template <>
    struct less<RoomArea> {
        bool operator()(const RoomArea& lhs, const RoomArea& rhs) const {
            return lhs.getStdStringViewUtf8() < rhs.getStdStringViewUtf8();
        }
    };
} // namespace std

MapData::MapData(QObject *const parent)
    : MapFrontend(parent)
{}

DoorName MapData::getDoorName(const RoomId id, const ExitDirEnum dir)
{
    if (dir < ExitDirEnum::UNKNOWN) {
        if (auto optDoorName = getCurrentMap().findDoorName(id, dir)) {
            return *optDoorName;
        }
    }

    static const DoorName tmp{"exit"};
    return tmp;
}

ExitDirFlags MapData::getExitDirections(const Coordinate &pos)
{
    if (const auto room = findRoomHandle(pos)) {
        return computeExitDirections(room.getRaw());
    }
    return {};
}

template<typename Callback>
static void walk_path(const RoomHandle &input_room, const CommandQueue &dirs, Callback &&callback)
{
    const auto &map = input_room.getMap();
    auto room = input_room; // Caution: This is reassigned below.
    for (const CommandEnum cmd : dirs) {
        if (cmd == CommandEnum::LOOK) {
            continue;
        }

        if (!isDirectionNESWUD(cmd)) {
            break;
        }

        const auto &e = room.getExit(getDirection(cmd));
        if (!e.exitIsExit()) {
            // REVISIT: why does this continue but all of the others break?
            continue;
        }

        // REVISIT: if it's more than one, why not just pick one?
        if (e.getOutgoingSet().size() != 1) {
            break;
        }

        const RoomId next = e.getOutgoingSet().first();

        // NOTE: reassignment.
        room = map.getRoomHandle(next);

        callback(room.getRaw());
    }
}

std::vector<Coordinate> MapData::getPath(const RoomId start, const CommandQueue &dirs) const
{
    if (start == INVALID_ROOMID) {
        return {};
    }

    std::vector<Coordinate> ret;
    ret.reserve(static_cast<size_t>(dirs.size()));

    const Map &map = getCurrentMap();
    if (const auto from = map.findRoomHandle(start)) {
        walk_path(from, dirs, [&ret](const RawRoom &room) { ret.push_back(room.getPosition()); });
    }
    return ret;
}

std::optional<RoomId> MapData::getLast(const RoomId start, const CommandQueue &dirs) const
{
    if (start == INVALID_ROOMID) {
        return std::nullopt;
    }

    std::optional<RoomId> ret;
    const Map &map = getCurrentMap();
    if (const auto from = map.findRoomHandle(start)) {
        walk_path(from, dirs, [&ret](const RawRoom &room) { ret = room.getId(); });
    }
    return ret;
}

FutureSharedMapBatchFinisher MapData::generateBatches(const mctp::MapCanvasTexturesProxy &textures)
{
    return generateMapDataFinisher(textures, getCurrentMap());
}

void MapData::applyChangesToList(const RoomSelection &sel,
                                 const std::function<Change(const RawRoom &)> &callback)
{
    ChangeList changes;
    for (const RoomId id : sel) {
        if (const auto ptr = findRoomHandle(id)) {
            changes.add(callback(ptr.getRaw()));
        }
    }
    applyChanges(changes);
}

void MapData::virt_clear()
{
    log("cleared MapData");
}

void MapData::removeDoorNames(ProgressCounter &pc)
{
    this->applySingleChange(pc, Change{world_change_types::RemoveAllDoorNames{}});
}

void MapData::generateBaseMap(ProgressCounter &pc)
{
    this->applySingleChange(pc, Change{world_change_types::GenerateBaseMap{}});
}

NODISCARD RoomIdSet MapData::genericFind(const RoomFilter &f) const
{
    return ::genericFind(getCurrentMap(), f);
}

MapData::~MapData() = default;

bool MapData::removeMarker(const InfomarkId id)
{
    try {
        auto db = getInfomarkDb();
        db.removeMarker(id);
        setCurrentMarks(db);
        return true;
    } catch (const std::runtime_error & /*ex*/) {
        return false;
    }
}

void MapData::removeMarkers(const MarkerList &toRemove)
{
    try {
        auto db = getInfomarkDb();
        for (const InfomarkId id : toRemove) {
            // REVISIT: try-catch around each one and report if any failed, instead of this all-or-nothing approach?
            db.removeMarker(id);
        }
        setCurrentMarks(db);
    } catch (const std::runtime_error &ex) {
        MMLOG() << "ERROR removing multiple infomarks: " << ex.what();
    }
}

InfomarkId MapData::addMarker(const InfoMarkFields &im)
{
    try {
        auto db = getInfomarkDb();
        auto id = db.addMarker(im);
        setCurrentMarks(db);
        return id;
    } catch (const std::runtime_error &ex) {
        MMLOG() << "ERROR adding infomark: " << ex.what();
        return INVALID_INFOMARK_ID;
    }
}

bool MapData::updateMarker(const InfomarkId id, const InfoMarkFields &im)
{
    try {
        auto db = getInfomarkDb();
        auto modified = db.updateMarker(id, im);
        if (modified) {
            setCurrentMarks(db, modified);
        }
        return true;
    } catch (const std::runtime_error &ex) {
        MMLOG() << "ERROR updating infomark: " << ex.what();
        return false;
    }
}

bool MapData::updateMarkers(const std::vector<InformarkChange> &updates)
{
    try {
        auto db = getInfomarkDb();
        auto modified = db.updateMarkers(updates);
        if (modified) {
            setCurrentMarks(db, modified);
        }
        return true;
    } catch (const std::runtime_error &ex) {
        MMLOG() << "ERROR updating infomarks: " << ex.what();
        return false;
    }
}

void MapData::slot_scheduleAction(const SigMapChangeList &change)
{
    this->applyChanges(change.deref());
}

void MapData::applyChanges(const ChangeList &changes)
{
    if (changes.empty()) {
        return;
    }

    // Apply changes to the map and notify observers (includes setting m_changedAfterSave)
    // This call handles getCurrentMap().applyChanges(internal_changes)
    MapFrontend::applyChanges(changes);

    // Ensure necessary headers are included, e.g. <variant>, <set>
    // <set> is already included. <variant> might be through Change.h, but explicit is good.
    // #include <variant> // Add this at the top of the file if not already present via other headers.

    std::set<RoomArea, std::less<RoomArea>> affectedAreas;
    bool globalRemeshNeeded = false;
    const Map& currentMap = getCurrentMap();

    auto addConnectedAreas = [&](const RoomHandle& roomHandle) {
        if (!roomHandle.isValid()) return;
        for (const ExitDirEnum dir : ALL_EXITS_NESWUD) {
            const auto& exit = roomHandle.getExit(dir);
            for (const RoomId& connectedRoomId : exit.getOutgoingSet()) {
                if (auto connectedRoomHandle = currentMap.findRoomHandle(connectedRoomId)) {
                    affectedAreas.insert(connectedRoomHandle.getArea());
                }
            }
            for (const RoomId& connectedRoomId : exit.getIncomingSet()) {
                if (auto connectedRoomHandle = currentMap.findRoomHandle(connectedRoomId)) {
                    affectedAreas.insert(connectedRoomHandle.getArea());
                }
            }
        }
    };

    auto visitorLambda = [&](const auto& specificChange) {
        using T = std::decay_t<decltype(specificChange)>;

        // World Changes
        // Use XFOREACH_WORLD_CHANGE as identified from existing visitor
        if constexpr (false
            #define CHECK_WORLD_CHANGE(Namespace, Type) || std::is_same_v<T, Namespace::Type>
            XFOREACH_WORLD_CHANGE(CHECK_WORLD_CHANGE)
            #undef CHECK_WORLD_CHANGE
        ) {
            globalRemeshNeeded = true;
        }
        // Room Changes
        else if constexpr (std::is_same_v<T, room_change_types::AddPermanentRoom>) {
            if (auto roomHandle = currentMap.findRoomByPosition(specificChange.position)) {
                affectedAreas.insert(roomHandle.getArea());
                addConnectedAreas(roomHandle);
            } else {
                globalRemeshNeeded = true;
            }
        } else if constexpr (std::is_same_v<T, room_change_types::AddRoom2>) {
            RoomId roomId = INVALID_ROOMID;
            // Attempt to get RoomId from event if 'event' member and 'getRoomId' method exist
            // This part is speculative based on plan; requires AddRoom2 structure knowledge
            // For example: if (specificChange.event.has_value()) roomId = specificChange.event->getRoomId();
            // For now, let's assume specificChange.id or specificChange.event.getRoomId() as per plan.
            // If AddRoom2 provides 'id' directly like 'AddRoom' did in old visitor:
            // roomId = specificChange.id; // This was an assumption for AddRoom.
            // Given the plan mentioned change.event.getRoomId() for AddRoom2:
            if constexpr (requires { specificChange.event.getRoomId(); }) { // Check if expression is valid
                 roomId = specificChange.event.getRoomId();
            }

            if (roomId == INVALID_ROOMID && specificChange.position) {
                 if (auto roomHandle = currentMap.findRoomByPosition(*specificChange.position)) {
                    roomId = roomHandle.getId();
                }
            }

            if (roomId != INVALID_ROOMID) {
                if (auto roomHandle = currentMap.findRoomHandle(roomId)) {
                    affectedAreas.insert(roomHandle.getArea());
                    addConnectedAreas(roomHandle);
                } else { globalRemeshNeeded = true; }
            } else {
                globalRemeshNeeded = true;
            }
        } else if constexpr (std::is_same_v<T, room_change_types::RemoveRoom>) {
                // Based on investigation, room_change_types::RemoveRoom only contains RoomId.
                // The room is already removed from currentMap when this visitor runs.
                // We cannot access its old area or exits without specificChange.data (RawRoom::ConstPtr), which is not present.
                // Therefore, the safest approach is to trigger a global remesh.
                // MMLOG() << "RemoveRoom change processed, triggering global remesh as pre-deletion data is not available in change struct.";
                globalRemeshNeeded = true;
        } else if constexpr (std::is_same_v<T, room_change_types::MakePermanent> ||
                             std::is_same_v<T, room_change_types::Update> ||
                             std::is_same_v<T, room_change_types::SetServerId> ||
                             std::is_same_v<T, room_change_types::MoveRelative> ||
                             std::is_same_v<T, room_change_types::TryMoveCloseTo>) {
            if (auto roomHandle = currentMap.findRoomHandle(specificChange.room)) {
                affectedAreas.insert(roomHandle.getArea());
                addConnectedAreas(roomHandle);
            } else { globalRemeshNeeded = true; }
        } else if constexpr (std::is_same_v<T, room_change_types::MoveRelative2>) {
            for (const RoomId roomId : specificChange.rooms) {
                if (auto roomHandle = currentMap.findRoomHandle(roomId)) {
                    affectedAreas.insert(roomHandle.getArea());
                    addConnectedAreas(roomHandle);
                } else { globalRemeshNeeded = true; if(globalRemeshNeeded) break; }
            }
             if (globalRemeshNeeded) { /* already broken or will be caught by outer loop */ }
        } else if constexpr (std::is_same_v<T, room_change_types::MergeRelative>) {
             if (auto roomHandle = currentMap.findRoomHandle(specificChange.room)) {
                affectedAreas.insert(roomHandle.getArea());
                addConnectedAreas(roomHandle);
            } else { globalRemeshNeeded = true; }
        } else if constexpr (std::is_same_v<T, room_change_types::ModifyRoomFlags>) {
            if (auto roomHandle = currentMap.findRoomHandle(specificChange.room)) {
                affectedAreas.insert(roomHandle.getArea());
                addConnectedAreas(roomHandle);
            } else { globalRemeshNeeded = true; }
        } else if constexpr (std::is_same_v<T, exit_change_types::ModifyExitConnection>) {
            bool fromRoomOk = false;
            if (auto fromRoomHandle = currentMap.findRoomHandle(specificChange.room)) {
                affectedAreas.insert(fromRoomHandle.getArea());
                fromRoomOk = true;
            } else { globalRemeshNeeded = true; }

            if (fromRoomOk && specificChange.to != INVALID_ROOMID) {
                 if (auto toRoomHandle = currentMap.findRoomHandle(specificChange.to)) {
                    affectedAreas.insert(toRoomHandle.getArea());
                } else { globalRemeshNeeded = true; }
            }
        } else if constexpr (std::is_same_v<T, exit_change_types::ModifyExitFlags> ||
                             std::is_same_v<T, exit_change_types::SetExitFlags> ||
                             std::is_same_v<T, exit_change_types::SetDoorFlags> ||
                             std::is_same_v<T, exit_change_types::SetDoorName>) {
            if (auto roomHandle = currentMap.findRoomHandle(specificChange.room)) {
                affectedAreas.insert(roomHandle.getArea());
                const auto& exit = roomHandle.getExit(specificChange.dir);
                for (const RoomId connectedRoomId : exit.getOutgoingSet()) {
                    if (auto connectedRoomHandle = currentMap.findRoomHandle(connectedRoomId)) {
                        affectedAreas.insert(connectedRoomHandle.getArea());
                    }
                }
                 for (const RoomId connectedRoomId : exit.getIncomingSet()) {
                    if (auto connectedRoomHandle = currentMap.findRoomHandle(connectedRoomId)) {
                        affectedAreas.insert(connectedRoomHandle.getArea());
                    }
                }
            } else { globalRemeshNeeded = true; }
        } else if constexpr (std::is_same_v<T, exit_change_types::NukeExit>) {
             if (auto roomHandle = currentMap.findRoomHandle(specificChange.room)) {
                affectedAreas.insert(roomHandle.getArea());
            } else { globalRemeshNeeded = true; }
        } else if constexpr (std::is_same_v<T, room_change_types::UndeleteRoom>) {
            affectedAreas.insert(specificChange.raw.getArea());
            if(auto roomHandle = currentMap.findRoomHandle(specificChange.raw.getId())) { // Room is already undeleted
                addConnectedAreas(roomHandle);
            } else { globalRemeshNeeded = true; }
        }
        else {
            // Default catch-all for any unhandled type from XFOREACH_CHANGE_TYPE
            globalRemeshNeeded = true;
        }
    }; // End of visitorLambda

    // Iterate over changes. Assuming 'changes' is ChangeList from MapFrontend,
    // and it's iterable, yielding 'ChangeWrapper' which has a 'change' member (e.g. Change*)
    for (const auto& changeWrapper : changes) { // Iterate directly over ChangeList if it's a list of ChangeWrappers
        // The original code was:
        // for (const auto& changeWrapper : changes) {
        //    if (changeWrapper.change) { // Ensure pointer is valid
        //        changeWrapper.change->accept(visitor);
        // So, changeWrapper.change is likely a pointer to an object that has an 'accept' method.
        // Given Change.h, this object is 'Change*' (or a compatible base if Change wasn't final).
        // Let's assume changeWrapper.change is Change*
        
        // If changeWrapper itself is the Change object (e.g. ChangeList is std::vector<Change>)
        // then it would be: changeWrapper.acceptVisitor(visitorLambda);
        // If ChangeList provides .getChanges() returning std::vector<ChangeVariantOwner*> where ChangeVariantOwner has acceptVisitor:
        // For now, using the structure from the previous attempt that seemed to match original iteration style:
        // for (const auto& changeWrapper : changes.getChanges()) { ... }
        // This implies changes.getChanges() returns the iterable collection.
        // And changeWrapper.change points to the visitable object.
        
        // Based on `Change.h` and typical usage of `std::variant` with visitors:
        // If `changeWrapper` is `const Change&` (i.e., `changes.getChanges()` returns `const std::vector<Change>&` or similar)
        // then `changeWrapper.acceptVisitor(visitorLambda);` is the correct call.
        // If `changeWrapper.change` is `Change*`:
        // then `changeWrapper.change->acceptVisitor(visitorLambda);`

        // The original code iterates `changes` directly: `for (const auto& changeWrapper : changes)`
        // And uses `changeWrapper.change->accept(visitor)`.
        // This structure suggests `changeWrapper` is an object that has a `change` member,
        // and this `change` member is a pointer to something that can accept a visitor.
        // Let's assume `changeWrapper.change` is a `Change*` (from Change.h).
        if (changeWrapper.change) { // Assuming changeWrapper has a 'change' member that is Change*
            changeWrapper.change->acceptVisitor(visitorLambda);
        } else {
            // This case might occur if changeWrapper.change can be null.
            // Depending on requirements, this might also warrant a global remesh or logging.
            // For now, assume valid changes or covered by existing null checks.
            // If it's a critical unhandled case, set globalRemeshNeeded = true;
        }

        if (globalRemeshNeeded) {
            break; 
        }
    }

    if (globalRemeshNeeded) {
        emit needsAreaRemesh({}); // Emit with empty set for global remesh
    } else if (!affectedAreas.empty()) {
        emit needsAreaRemesh(affectedAreas);
    }
    // No explicit 'else' here, as MapFrontend::applyChanges already signals a generic data change.
    // This new signal is for more targeted remeshing.
}

bool MapData::isEmpty() const
{
    // return (greatestUsedId == INVALID_ROOMID) && m_markers.empty();
    return getCurrentMap().empty() && getInfomarkDb().empty();
}

void MapData::removeMissing(RoomIdSet &set) const
{
    RoomIdSet invalid;

    for (const RoomId id : set) {
        if (!findRoomHandle(id)) {
            invalid.insert(id);
        }
    }

    for (const RoomId id : invalid) {
        set.erase(id);
    }
}

void MapData::setMapData(const MapLoadData &mapLoadData)
{
    // TODO: make all of this an all-or-nothing commit;
    // for now the catch + abort has that effect.
    try {
        MapFrontend &mf = *this;
        mf.block();
        {
            InfomarkDb markers = mapLoadData.markerData;
            setFileName(mapLoadData.filename, mapLoadData.readonly);
            setSavedMap(mapLoadData.mapPair.base);
            setCurrentMap(mapLoadData.mapPair.modified);
            setCurrentMarks(markers);
            setSavedMarks(markers);
            forcePosition(mapLoadData.position);

            // NOTE: The map may immediately report changes.
        }
        mf.unblock();
    } catch (...) {
        // REVISIT: should this be fatal, or just throw?
        qFatal("An exception occured while setting the map data.");
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunreachable-code"
#endif
        // the following is in case qFatal() isn't actually noreturn.
        // NOLINTNEXTLINE
        std::abort();
#ifdef __clang__
#pragma clang diagnostic pop
#endif
    }
}

// TODO: implement a better merge!
// The old "merge" algorithm was really unsophisticated;
// it just inserted map into with a position and ID offset.
//
// A better approach would be to look for the common subset,
// and then look for and prompt the user to approve changes like:
//  * typo-fixes
//  * flag changes
//  * added or removed door names
//  * added / removed connections within the common subset
//
// Finally, accept any additions, but do so at offset and nextid.
std::pair<Map, InfomarkDb> MapData::mergeMapData(ProgressCounter &counter,
                                                 const Map &currentMap,
                                                 const InfomarkDb &currentMarks,
                                                 RawMapLoadData newMapData)
{
    const Bounds newBounds = [&newMapData]() {
        const auto &rooms = newMapData.rooms;
        const auto &front = rooms.front().position;
        Bounds bounds{front, front};
        for (const auto &room : rooms) {
            bounds.insert(room.getPosition());
        }
        return bounds;
    }();

    const Coordinate mapOffset = [&currentMap, &newBounds]() -> Coordinate {
        const auto currentBounds = currentMap.getBounds().value();

        // NOTE: current and new map origins may not be at the same place relative to the bounds,
        // so we'll use the upper bound of the current map and the lower bound of the new map
        // to compute the offset.

        constexpr int margin = 1;
        Coordinate tmp = currentBounds.max - newBounds.min + Coordinate{1, 1, 0} * margin;
        // NOTE: The reason it's at a z=-1 offset is so the manual "merge up" command will work.
        tmp.z = -1;

        return tmp;
    }();

    const auto infomarkOffset = [&mapOffset]() -> Coordinate {
        const auto tmp = mapOffset.to_ivec3() * glm::ivec3{INFOMARK_SCALE, INFOMARK_SCALE, 1};
        return Coordinate{tmp.x, tmp.y, tmp.z};
    }();

    const Map newMap = Map::merge(counter, currentMap, std::move(newMapData.rooms), mapOffset);

    const InfomarkDb newMarks = [&newMapData, &currentMarks, &infomarkOffset, &counter]() {
        auto tmp = currentMarks;
        if (newMapData.markerData) {
            const auto &markers = newMapData.markerData.value().markers;
            counter.setNewTask(ProgressMsg{"adding infomarks"}, markers.size());
            for (const InfoMarkFields &mark : markers) {
                auto copy = mark.getOffsetCopy(infomarkOffset);
                std::ignore = tmp.addMarker(copy);
                counter.step();
            }
        }
        return tmp;
    }();

    return std::pair<Map, InfomarkDb>(newMap, newMarks);
}

void MapData::describeChanges(std::ostream &os) const
{
    if (!MapFrontend::isModified()) {
        os << "No changes since the last save.\n";
        return;
    }

    {
        const Map &savedMap = getSavedMap();
        const Map &currentMap = getCurrentMap();
        if (savedMap != currentMap) {
            const auto stats = getBasicDiffStats(savedMap, currentMap);
            auto printRoomDiff = [&os](const std::string_view what, const size_t count) {
                if (count == 0) {
                    return;
                }
                os << "Rooms " << what << ": " << count << ".\n";
            };
            printRoomDiff("removed", stats.numRoomsRemoved);
            printRoomDiff("added", stats.numRoomsAdded);
            printRoomDiff("changed", stats.numRoomsChanged);
        }

        if (getSavedMarks() != getCurrentMarks()) {
            // REVISIT: Can we get a better description of what changed?
            os << "Infomarks have changed.\n";
        }

        // REVISIT: Should we also include the time of the last update?
    }
}

std::string MapData::describeChanges() const
{
    std::ostringstream os;
    describeChanges(os);
    return std::move(os).str();
}
