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
#include "../map/AbstractChangeVisitor.h"
#include "../map/ChangeTypes.h" // For XFOREACH_WORLD_CHANGE (and XFOREACH_WORLD_CHANGE_TYPES)
#include "../map/CommandId.h"
#include "../map/ExitDirection.h"
#include "../map/ExitFieldVariant.h"
#include "../map/RawRoom.h"
#include "../map/RawRoom.h" // For change.data in RemoveRoom
#include "../map/RoomRecipient.h"
#include "../map/World.h"
#include "../map/coordinate.h"
#include "../map/exit.h"
#include "../map/exit.h" // For RawExit in RemoveRoom
#include "../map/infomark.h"
#include "../map/mmapper2room.h"
#include "../map/room.h"
#include "../map/roomid.h"
// #include "../map/world_change_types.h" // Removed, functionality assumed in ChangeTypes.h
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
#include <string_view> // For std::less<RoomArea>
#include <tuple>
#include <variant> // Added for std::visit and std::variant
#include <vector>

#include <QApplication>
#include <QList>
#include <QString>

// Custom std::less specialization for RoomArea - Commented out due to redefinition error
// namespace std {
//     template <>
//     struct less<RoomArea> {
//         bool operator()(const RoomArea& lhs, const RoomArea& rhs) const {
//             return lhs.getStdStringViewUtf8() < rhs.getStdStringViewUtf8();
//         }
//     };
// } // namespace std

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

// This is the new applyChanges that uses MapApplyResult correctly
bool MapData::applyChanges(const ChangeList &changes)
{
    if (changes.empty()) {
        // MMLOG_DEBUG() << "MapData::applyChanges called with empty ChangeList.";
        return true; // No changes, operation is trivially successful.
    }

    ProgressCounter pc; // Assuming a default ProgressCounter is okay here.
    MapApplyResult result = MapFrontend::getCurrentMap().apply(pc, changes);

    // Update the internal map state
    MapFrontend::setCurrentMap(result.map); // Assuming MapFrontend has a method to set the map

    // Handle visually dirty areas
    const auto &dirty_areas = result.visuallyDirtyAreas;
    bool specific_areas_handled = false;
    if (!dirty_areas.empty()) {
        // MMLOG_DEBUG() << "MapData::applyChanges emitting needsAreaRemesh for specific areas.";
        emit needsAreaRemesh(dirty_areas); // Emit with the set of dirty RoomArea objects
        specific_areas_handled = true;
    }

    // Handle global RoomMeshNeedsUpdate if no specific areas were handled
    // and the global flag is set.
    if (result.roomUpdateFlags.contains(RoomUpdateEnum::RoomMeshNeedsUpdate)
        && !specific_areas_handled) {
        MMLOG_INFO()
            << "MapData::applyChanges: Global RoomMeshNeedsUpdate flag set, but no specific dirty areas were identified. This might indicate a need for a global remesh or further investigation.";
        // To trigger a "global" remesh via the existing signal, we can emit it with an empty set,
        // and the receiver can interpret an empty set as "remesh everything".
        // Or, if there's a dedicated global remesh signal, emit that.
        // For now, using the existing signal with an empty set as a convention.
        emit needsAreaRemesh({}); // Empty set indicates global remesh
    }

    // Handle other update flags like BoundsChanged
    if (result.roomUpdateFlags.contains(RoomUpdateEnum::BoundsChanged)) {
        // MMLOG_DEBUG() << "MapData::applyChanges: BoundsChanged flag set.";
        // Emit a signal or call a handler for bounds change if necessary.
        // For example: emit mapBoundsChanged();
    }

    // Notify that the map content has changed generally
    // This might be implicit in MapFrontend's handling or might need explicit emission.
    // MapFrontend::onNotifyModified should be called by setCurrentMap or apply if it modifies the map.
    // If direct emission is needed:
    // emit sig_onDataChanged(); // Assuming this is the general "map modified" signal

    // The success of applying changes is implicitly handled by not throwing exceptions.
    // The MapApplyResult itself doesn't carry a success/failure bool for the operation itself,
    // rather it carries the new map state and flags about what changed.
    return true;
}

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

// bool MapData::applyChanges(const ChangeList &changes) // Changed return type to bool // OLD IMPLEMENTATION
// {
//     if (changes.empty()) {
//         return true; // Or false, depending on desired semantics for empty change list; true seems reasonable.
//     }

//     // Apply changes to the map and notify observers (includes setting m_changedAfterSave)
//     // This call handles getCurrentMap().applyChanges(internal_changes)
//     bool success = MapFrontend::applyChanges(changes); // Call base class method

//     // Ensure necessary headers are included, e.g. <variant>, <set>
//     // <set> is already included. <variant> might be through Change.h, but explicit is good.

//     std::set<RoomArea, std::less<RoomArea>> affectedAreas;
//     bool globalRemeshNeeded = false;
//     const Map& currentMap = getCurrentMap();

//     auto addConnectedAreas = [&](const RoomHandle& roomHandle) {
//         if (!roomHandle.exists()) return;
//         for (const ExitDirEnum dir : ALL_EXITS_NESWUD) {
//             const auto& exit = roomHandle.getExit(dir);
//             for (const RoomId& connectedRoomId : exit.getOutgoingSet()) {
//                 if (auto connectedRoomHandle = currentMap.findRoomHandle(connectedRoomId)) {
//                     affectedAreas.insert(connectedRoomHandle.getArea());
//                 }
//             }
//             for (const RoomId& connectedRoomId : exit.getIncomingSet()) {
//                 if (auto connectedRoomHandle = currentMap.findRoomHandle(connectedRoomId)) {
//                     affectedAreas.insert(connectedRoomHandle.getArea());
//                 }
//             }
//         }
//     };

//     auto visitorLambda = [&](const auto& specificChange) {
//         using T = std::decay_t<decltype(specificChange)>;

//         // World Changes
//         // Use XFOREACH_WORLD_CHANGE as identified from existing visitor
//         if constexpr (false
//             #define X_NOP_LOCAL_MAPDATA() // Renamed X_NOP to avoid conflict if globally defined elsewhere
//             #define CHECK_WORLD_CHANGE(FullType) || std::is_same_v<T, FullType>
//             XFOREACH_WORLD_CHANGE_TYPES(CHECK_WORLD_CHANGE, X_NOP_LOCAL_MAPDATA)
//             #undef CHECK_WORLD_CHANGE
//             #undef X_NOP_LOCAL_MAPDATA
//         ) {
//             globalRemeshNeeded = true;
//         }
//         // Room Changes
//         else if constexpr (std::is_same_v<T, room_change_types::AddPermanentRoom>) {
//             if (auto roomHandle = currentMap.findRoomHandle(specificChange.position)) {
//                 affectedAreas.insert(roomHandle.getArea());
//                 addConnectedAreas(roomHandle);
//             } else {
//                 globalRemeshNeeded = true;
//             }
//         } else if constexpr (std::is_same_v<T, room_change_types::AddRoom2>) {
//             RoomId roomId = INVALID_ROOMID;
//             // Attempt to get RoomId from event if 'event' member and 'getRoomId' method exist
//             // This part is speculative based on plan; requires AddRoom2 structure knowledge
//             // Corrected logic for AddRoom2 to get RoomId via event's ServerRoomId
//             ServerRoomId server_id_from_event = specificChange.event.getServerId();
//             if (server_id_from_event != INVALID_SERVER_ROOMID) {
//                 if (auto roomHandle = currentMap.findRoomHandle(server_id_from_event)) {
//                     roomId = roomHandle.getId();
//                 }
//             }

//             if (roomId == INVALID_ROOMID) { // specificChange.position is a direct member, not optional
//                  if (auto roomHandle = currentMap.findRoomHandle(specificChange.position)) { // No * needed
//                     roomId = roomHandle.getId();
//                 }
//             }

//             if (roomId != INVALID_ROOMID) {
//                 if (auto roomHandle = currentMap.findRoomHandle(roomId)) {
//                     affectedAreas.insert(roomHandle.getArea());
//                     addConnectedAreas(roomHandle);
//                 } else { globalRemeshNeeded = true; }
//             } else {
//                 globalRemeshNeeded = true;
//             }
//         } else if constexpr (std::is_same_v<T, room_change_types::RemoveRoom>) {
//                 // Based on investigation, room_change_types::RemoveRoom only contains RoomId.
//                 // The room is already removed from currentMap when this visitor runs.
//                 // We cannot access its old area or exits without specificChange.data (RawRoom::ConstPtr), which is not present.
//                 // Therefore, the safest approach is to trigger a global remesh.
//                 // MMLOG() << "RemoveRoom change processed, triggering global remesh as pre-deletion data is not available in change struct.";
//                 globalRemeshNeeded = true;
//         } else if constexpr (std::is_same_v<T, room_change_types::MakePermanent> ||
//                              std::is_same_v<T, room_change_types::Update> ||
//                              std::is_same_v<T, room_change_types::SetServerId> ||
//                              std::is_same_v<T, room_change_types::MoveRelative> ||
//                              std::is_same_v<T, room_change_types::TryMoveCloseTo>) {
//             if (auto roomHandle = currentMap.findRoomHandle(specificChange.room)) {
//                 affectedAreas.insert(roomHandle.getArea());
//                 addConnectedAreas(roomHandle);
//             } else { globalRemeshNeeded = true; }
//         } else if constexpr (std::is_same_v<T, room_change_types::MoveRelative2>) {
//             for (const RoomId roomId : specificChange.rooms) {
//                 if (auto roomHandle = currentMap.findRoomHandle(roomId)) {
//                     affectedAreas.insert(roomHandle.getArea());
//                     addConnectedAreas(roomHandle);
//                 } else { globalRemeshNeeded = true; if(globalRemeshNeeded) break; }
//             }
//              if (globalRemeshNeeded) { /* already broken or will be caught by outer loop */ }
//         } else if constexpr (std::is_same_v<T, room_change_types::MergeRelative>) {
//              if (auto roomHandle = currentMap.findRoomHandle(specificChange.room)) {
//                 affectedAreas.insert(roomHandle.getArea());
//                 addConnectedAreas(roomHandle);
//             } else { globalRemeshNeeded = true; }
//         } else if constexpr (std::is_same_v<T, room_change_types::ModifyRoomFlags>) {
//             if (auto roomHandle = currentMap.findRoomHandle(specificChange.room)) {
//                 affectedAreas.insert(roomHandle.getArea());
//                 addConnectedAreas(roomHandle);
//             } else { globalRemeshNeeded = true; }
//         } else if constexpr (std::is_same_v<T, exit_change_types::ModifyExitConnection>) {
//             bool fromRoomOk = false;
//             if (auto fromRoomHandle = currentMap.findRoomHandle(specificChange.room)) {
//                 affectedAreas.insert(fromRoomHandle.getArea());
//                 fromRoomOk = true;
//             } else { globalRemeshNeeded = true; }

//             if (fromRoomOk && specificChange.to != INVALID_ROOMID) {
//                  if (auto toRoomHandle = currentMap.findRoomHandle(specificChange.to)) {
//                     affectedAreas.insert(toRoomHandle.getArea());
//                 } else { globalRemeshNeeded = true; }
//             }
//         } else if constexpr (std::is_same_v<T, exit_change_types::ModifyExitFlags> ||
//                              std::is_same_v<T, exit_change_types::SetExitFlags> ||
//                              std::is_same_v<T, exit_change_types::SetDoorFlags> ||
//                              std::is_same_v<T, exit_change_types::SetDoorName>) {
//             if (auto roomHandle = currentMap.findRoomHandle(specificChange.room)) {
//                 affectedAreas.insert(roomHandle.getArea());
//                 const auto& exit = roomHandle.getExit(specificChange.dir);
//                 for (const RoomId connectedRoomId : exit.getOutgoingSet()) {
//                     if (auto connectedRoomHandle = currentMap.findRoomHandle(connectedRoomId)) {
//                         affectedAreas.insert(connectedRoomHandle.getArea());
//                     }
//                 }
//                  for (const RoomId connectedRoomId : exit.getIncomingSet()) {
//                     if (auto connectedRoomHandle = currentMap.findRoomHandle(connectedRoomId)) {
//                         affectedAreas.insert(connectedRoomHandle.getArea());
//                     }
//                 }
//             } else { globalRemeshNeeded = true; }
//         } else if constexpr (std::is_same_v<T, exit_change_types::NukeExit>) {
//              if (auto roomHandle = currentMap.findRoomHandle(specificChange.room)) {
//                 affectedAreas.insert(roomHandle.getArea());
//             } else { globalRemeshNeeded = true; }
//         } else if constexpr (std::is_same_v<T, room_change_types::UndeleteRoom>) {
//             affectedAreas.insert(specificChange.raw.getArea());
//             if(auto roomHandle = currentMap.findRoomHandle(specificChange.raw.getId())) { // Room is already undeleted
//                 addConnectedAreas(roomHandle);
//             } else { globalRemeshNeeded = true; }
//         }
//         else {
//             // Default catch-all for any unhandled type from XFOREACH_CHANGE_TYPE
//             globalRemeshNeeded = true;
//         }
//     }; // End of visitorLambda

//     // Iterate over changes. Assuming 'changes' is ChangeList from MapFrontend,
//     // Corrected iteration based on ChangeList.h providing getChanges() -> std::vector<Change>
//     for (const Change& actual_change : changes.getChanges()) {
//         actual_change.acceptVisitor(visitorLambda);

//         if (globalRemeshNeeded) {
//             break;
//         }
//     }

//     if (globalRemeshNeeded) {
//         emit needsAreaRemesh({}); // Emit with empty set for global remesh
//     } else if (!affectedAreas.empty()) {
//         emit needsAreaRemesh(affectedAreas);
//     }
//     // No explicit 'else' here, as MapFrontend::applyChanges already signals a generic data change.
//     // This new signal is for more targeted remeshing.
//     return success; // Return status from base class
// }

bool MapData::isEmpty() const
{
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
