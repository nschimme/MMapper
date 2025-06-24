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
#include "../map/World.h"
#include "../map/coordinate.h"
#include "../map/exit.h"
#include "../map/infomark.h"
#include "../map/mmapper2room.h"
#include "../map/room.h"
#include "../map/roomid.h"
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

FutureSharedMapBatchFinisher MapData::generateBatches(const mctp::MapCanvasTexturesProxy &textures,
                                                      const std::shared_ptr<const FontMetrics> &font)
{
    return generateMapDataFinisher(textures, font, getCurrentMap());
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
    return applySingleChange(Change{infomark_change_types::RemoveInfomark{id}});
}

void MapData::removeMarkers(const MarkerList &toRemove)
{
    if (toRemove.empty()) {
        return;
    }
    ChangeList changes;
    for (const InfomarkId id : toRemove) {
        changes.add(Change{infomark_change_types::RemoveInfomark{id}});
    }
    applyChanges(changes);
}

bool MapData::addMarker(const InfoMarkFields &im)
{
    // The method now returns bool indicating success of applying the change.
    // The actual InfomarkId is not directly available from this operation anymore.
    // If needed, it would have to be queried or determined differently.
    bool success = applySingleChange(Change{infomark_change_types::AddInfomark{im}});
    if (!success) {
        MMLOG() << "ERROR applying AddInfomark change.";
        // Potentially return an invalid ID or handle error appropriately
    }
    return success;
}

bool MapData::updateMarker(const InfomarkId id, const InfoMarkFields &im)
{
    return applySingleChange(Change{infomark_change_types::UpdateInfomark{id, im}});
}

// updateMarkers was removed

void MapData::slot_scheduleAction(const SigMapChangeList &change)
{
    this->applyChanges(change.deref());
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
            // InfomarkDb markers = mapLoadData.markerData; // Removed
            setFileName(mapLoadData.filename, mapLoadData.readonly);
            setSavedMap(mapLoadData.mapPair.base);
            // setCurrentMarks(markers); // Removed
            // setSavedMarks(markers); // Removed

            // Note: mapLoadData.markerData needs to be loaded into the World
            // This will require changes elsewhere, possibly World::init or Map::fromRooms,
            // or applying a new Change type after map creation.
            // For now, we just set the map without the old marker logic.
            setCurrentMap(mapLoadData.mapPair.modified);
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
Map MapData::mergeMapData(ProgressCounter &counter,
                          const Map &currentMap,
                          // const InfomarkDb &currentMarks, // Removed
                          RawMapLoadData newMapData)
{
    // InfomarkDb merged_marks = currentMap.getInfomarkDb(); // Will apply as changes

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

    const Map newMapWithoutMergedInfomarks = Map::merge(counter, currentMap, std::move(newMapData.rooms), mapOffset);

    ChangeList infomark_add_changes;
    if (newMapData.markerData) {
        const auto &markers_from_new_data = newMapData.markerData.value().markers;
        counter.setNewTask(ProgressMsg{"preparing infomark merge changes"}, markers_from_new_data.size());
        for (const InfoMarkFields &mark_fields : markers_from_new_data) {
            InfoMarkFields offset_mark_fields = mark_fields.getOffsetCopy(infomarkOffset);
            infomark_add_changes.add(Change{infomark_change_types::AddInfomark{offset_mark_fields}});
            counter.step();
        }
    }

    // Apply infomark changes from the new map data onto the merged map structure
    // Also, existing infomarks from currentMap are already part of newMapWithoutMergedInfomarks.
    // If the intent is to *only* have currentMarks + newMapData.markerData,
    // then currentMap's original infomarks should first be cleared from newMap if Map::merge doesn't do that.
    // Assuming Map::merge preserves currentMap's infomarks, and we just add new ones.
    // If newMapData.markerData should *replace* existing markers in its spatial region,
    // that would be a more complex operation (e.g. RemoveInfomark for existing ones in the region, then AddInfomark).
    // The current approach simply adds all markers from newMapData.markerData to whatever newMap contains.

    Map map_with_infomarks = newMapWithoutMergedInfomarks; // Start with merged map (includes original infomarks)
    if (!infomark_add_changes.getChanges().empty()) {
         MapApplyResult result = newMapWithoutMergedInfomarks.apply(counter, infomark_add_changes);
         map_with_infomarks = result.map;
    }

    return map_with_infomarks;
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
