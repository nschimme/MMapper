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

namespace { // Anonymous namespace for helper classes

struct AreaCollectorVisitor : public AbstractChangeVisitor {
    std::set<RoomArea>& impactedAreas;
    const Map& currentMap; // Should be the map *after* changes have been applied for most cases
    bool& requiresGlobalRemesh;

    AreaCollectorVisitor(std::set<RoomArea>& areas, const Map& map, bool& globalRemeshFlag)
        : impactedAreas(areas), currentMap(map), requiresGlobalRemesh(globalRemeshFlag) {}

    // --- Visit methods for specific change types ---

    void visit(const change_types::RoomPropertySet& change) override {
        if (auto roomHandle = currentMap.findRoomHandle(change.room)) {
            impactedAreas.insert(roomHandle.getArea());
            addConnectedAreas(roomHandle);
        }
        // Note: If RoomArea itself is changed, this gets the *new* area.
        // Remeshing the *old* area is a potential improvement if change.old_value (e.g. change.data.old_area)
        // was available and could be added to impactedAreas.
    }

    void visit(const change_types::ExitPropertySet& change) override {
        if (auto roomHandle = currentMap.findRoomHandle(change.room)) {
            impactedAreas.insert(roomHandle.getArea());
            addConnectedAreas(roomHandle);
        }
    }
    
    void visit(const change_types::AddRoom& change) override {
        // currentMap already has the new room because MapFrontend::applyChanges was called before visitor
        if (auto roomHandle = currentMap.findRoomHandle(change.id)) {
            impactedAreas.insert(roomHandle.getArea());
            // For AddRoom, connected areas will be handled if AddExit changes follow.
            // If AddRoom implies default exits to existing rooms, those rooms might need updates
            // but that logic would be complex to add here without more context on AddRoom's side effects.
        }
    }

    void visit(const change_types::RemoveRoom& change) override {
        // change.data is the RawRoom object *before* deletion.
        if (change.data) { // Ensure data is present
            impactedAreas.insert(change.data->getArea()); 
            // Add areas of rooms that were connected to the deleted room.
            // These connections are taken from change.data (pre-deletion state).
            for (const ExitDirEnum dir : ALL_EXITS_NESWUD) {
                const auto& exitData = change.data->getExit(dir); // RawExit from pre-delete state
                for (const RoomId& connectedId : exitData.getOutgoingSet()) {
                    if (auto connectedRoom = currentMap.findRoomHandle(connectedId)) { // Check if neighbor still exists
                        impactedAreas.insert(connectedRoom.getArea());
                    }
                }
                for (const RoomId& connectedId : exitData.getIncomingSet()) {
                     if (auto connectedRoom = currentMap.findRoomHandle(connectedId)) { // Check if neighbor still exists
                        impactedAreas.insert(connectedRoom.getArea());
                    }
                }
            }
        }
    }

    void visit(const change_types::AddExit& change) override {
        if (auto fromRoom = currentMap.findRoomHandle(change.from)) {
            impactedAreas.insert(fromRoom.getArea());
        }
        if (auto toRoom = currentMap.findRoomHandle(change.to)) {
            impactedAreas.insert(toRoom.getArea());
        }
    }

    void visit(const change_types::RemoveExit& change) override {
        // For RemoveExit, change.from and change.to are RoomIds.
        // The exit is already removed from currentMap when the visitor runs.
        // We add the areas of the rooms that were connected by this exit.
        if (auto fromRoom = currentMap.findRoomHandle(change.from)) {
            impactedAreas.insert(fromRoom.getArea());
        }
        if (auto toRoom = currentMap.findRoomHandle(change.to)) {
            impactedAreas.insert(toRoom.getArea());
        }
        // If change.data (containing old RawExit) were available, we could also find
        // other rooms connected *through* this exit if it was part of a multi-room connection,
        // but that's a more complex scenario.
    }

    // Handle WorldChange types to flag for global remesh
    #define X(Namespace, Type) void visit(const Namespace::Type& /*change*/) override { requiresGlobalRemesh = true; }
    XFOREACH_WORLD_CHANGE(X)
    #undef X
    
    // Default for unhandled changes: assume global remesh to be safe, or log.
    // For now, let's be conservative. Most non-world changes are covered above.
    // If a new AbstractChange is added and not handled, it might slip through.
    void defaultCase(const AbstractChange& /*change*/) override {
        // Consider logging an unhandled change type here.
        // For safety, triggering a global remesh for unknown changes might be an option,
        // but it could also hide issues. For now, specific changes trigger area remesh.
        // If a change doesn't fall into a specific category and isn't a WorldChange,
        // it won't trigger area-specific or global remesh via this visitor.
        // This relies on the existing full remesh being triggered by `setDataChanged()`.
    }

private:
    void addConnectedAreas(const RoomHandle& roomHandle) {
        for (const ExitDirEnum dir : ALL_EXITS_NESWUD) {
            // Use getExit to get the current state of exits in the room.
            const auto& exit = roomHandle.getExit(dir); 
            for (const RoomId& connectedRoomId : exit.getOutgoingSet()) {
                if (auto connectedRoomHandle = currentMap.findRoomHandle(connectedRoomId)) {
                    impactedAreas.insert(connectedRoomHandle.getArea());
                }
            }
            // Also consider incoming for completeness, though often changes to an exit
            // on one side imply the other side is also being handled if it's a two-way exit.
            for (const RoomId& connectedRoomId : exit.getIncomingSet()) {
                if (auto connectedRoomHandle = currentMap.findRoomHandle(connectedRoomId)) {
                    impactedAreas.insert(connectedRoomHandle.getArea());
                }
            }
        }
    }
};

} // namespace

void MapData::applyChanges(const ChangeList &changes)
{
    if (changes.empty()) {
        return;
    }

    // Apply changes to the map and notify observers (includes setting m_changedAfterSave)
    // This call handles getCurrentMap().applyChanges(internal_changes)
    MapFrontend::applyChanges(changes);

    std::set<RoomArea> impactedAreas;
    bool requiresGlobalRemesh = false;
    
    // getCurrentMap() now reflects the state *after* the changes.
    AreaCollectorVisitor visitor(impactedAreas, getCurrentMap(), requiresGlobalRemesh);

    for (const auto& changeWrapper : changes) { // Assuming ChangeList holds Ptr<Change> or similar
        if (changeWrapper.change) { // Ensure pointer is valid
            changeWrapper.change->accept(visitor);
            if (requiresGlobalRemesh) { // If a global change is found, stop collecting.
                break;
            }
        }
    }

    if (requiresGlobalRemesh) {
        emit needsAreaRemesh({}); // Emit with empty set for global remesh
    } else if (!impactedAreas.empty()) {
        emit needsAreaRemesh(impactedAreas);
    }
    // If neither global nor specific areas, the regular full remesh triggered by
    // MapFrontend::applyChanges (via setDataChanged -> sig_onDataChanged) will handle it.
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
