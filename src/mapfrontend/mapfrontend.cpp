// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Ulf Hermann <ulfonk_mennhar@gmx.de> (Alve)
// Author: Marek Krejza <krejza@gmail.com> (Caligor)
// Author: Nils Schimmelmann <nschimme@gmail.com> (Jahara)

#include "mapfrontend.h"

#include "../global/SendToUser.h"
#include "../global/Timer.h"
#include "../global/logging.h"
#include "../global/progresscounter.h"
#include "../map/ChangeTypes.h"
#include "../map/coordinate.h"
#include "../map/parseevent.h"
#include "../map/room.h"
#include "../map/roomid.h"

#include <cassert>
#include <memory>
#include <set>
#include <tuple>
#include <utility>

static const size_t MAX_UNDO_HISTORY_CONST = 100;

MapFrontend::MapFrontend(QObject *const parent)
    : QObject(parent)
    , m_history(MAX_UNDO_HISTORY_CONST)
{
    emitUndoRedoAvailability();
}

MapFrontend::~MapFrontend()
{
    emit sig_clearingMap();
}

void MapFrontend::block()
{
    blockSignals(true);
}

void MapFrontend::unblock()
{
    blockSignals(false);
}

void MapFrontend::checkSize()
{
    const auto bounds = getCurrentMap().getBounds().value_or(Bounds{});
    emit sig_mapSizeChanged(bounds.min, bounds.max);
}

void MapFrontend::scheduleAction(const Change &change)
{
    std::ignore = applySingleChange(change);
}

void MapFrontend::revert()
{
    if (!m_current.map.empty() || m_history.isUndoAvailable()) {
        m_history.recordChange(m_current.map);
    }
    emit sig_clearingMap();
    setCurrentMap(MapApplyResult{m_saved.map});
    currentHasBeenSaved();
}

void MapFrontend::clear()
{
    if (!m_current.map.empty() || m_history.isUndoAvailable()) {
        qInfo() << "recorded change";
        m_history.recordChange(m_current.map);
    }

    emit sig_clearingMap();
    setCurrentMap(MapApplyResult{Map{}});
    currentHasBeenSaved();
    virt_clear();
}

bool MapFrontend::createEmptyRoom(const Coordinate &c)
{
    const auto &map = getCurrentMap();
    if (map.findRoomHandle(c)) {
        MMLOG_ERROR() << "A room already exists at the chosen position.";
        global::sendToUser("A room already exists at the chosen position.\n");
        return false;
    }

    return applySingleChange(Change{room_change_types::AddPermanentRoom{c}});
}

bool MapFrontend::hasTemporaryRoom(const RoomId id) const
{
    if (RoomHandle rh = getCurrentMap().findRoomHandle(id)) {
        return rh.isTemporary();
    }
    return false;
}

bool MapFrontend::tryRemoveTemporary(const RoomId id)
{
    if (hasTemporaryRoom(id)) {
        return applySingleChange(Change{room_change_types::RemoveRoom{id}});
    }
    return false;
}

bool MapFrontend::tryMakePermanent(const RoomId id)
{
    if (hasTemporaryRoom(id)) {
        return applySingleChange(Change{room_change_types::MakePermanent{id}});
    }
    return false;
}

void MapFrontend::slot_createRoom(const SigParseEvent &sigParseEvent,
                                  const Coordinate &expectedPosition)
{
    const ParseEvent &event = sigParseEvent.deref();

    MMLOG() << "[mapfrontend] Adding new room from parseEvent";

    const bool success = applySingleChange(
        Change{room_change_types::AddRoom2{expectedPosition, event}});

    if (success) {
        MMLOG() << "[mapfrontend] Added new room.";
    } else {
        MMLOG() << "[mapfrontend] Failed to add new room.";
    }
}

RoomHandle MapFrontend::findRoomHandle(RoomId id) const
{
    return getCurrentMap().findRoomHandle(id);
}

RoomHandle MapFrontend::findRoomHandle(const Coordinate &coord) const
{
    return getCurrentMap().findRoomHandle(coord);
}

RoomHandle MapFrontend::findRoomHandle(const ExternalRoomId id) const
{
    return getCurrentMap().findRoomHandle(id);
}

RoomHandle MapFrontend::findRoomHandle(const ServerRoomId id) const
{
    return getCurrentMap().findRoomHandle(id);
}

RoomHandle MapFrontend::getRoomHandle(const RoomId id) const
{
    return getCurrentMap().getRoomHandle(id);
}

const RawRoom &MapFrontend::getRawRoom(const RoomId id) const
{
    return getCurrentMap().getRawRoom(id);
}

RoomIdSet MapFrontend::findAllRooms(const Coordinate &coord) const
{
    if (const auto room = findRoomHandle(coord)) {
        return RoomIdSet{room.getId()};
    }
    return RoomIdSet{};
}

RoomIdSet MapFrontend::findAllRooms(const SigParseEvent &event) const
{
    if (!event.isValid()) {
        return RoomIdSet{};
    }
    return getCurrentMap().findAllRooms(event.deref());
}

RoomIdSet MapFrontend::findAllRooms(const Coordinate &input_min, const Coordinate &input_max) const
{
    Bounds bounds{input_min, input_max};
    RoomIdSet result;
    const auto &map = getCurrentMap();
    map.getRooms().for_each([&](const RoomId id) {
        const auto &r = map.getRoomHandle(id);
        if (bounds.contains(r.getPosition())) {
            result.insert(r.getId());
        }
    });
    return result;
}

RoomIdSet MapFrontend::lookingForRooms(const SigParseEvent &sigParseEvent)
{
    const ParseEvent &event = sigParseEvent.deref();
    return getCurrentMap().findAllRooms(event);
}

void MapFrontend::setSavedMap(Map map)
{
    m_saved.map = map;
    m_snapshot.map = map;
}

void MapFrontend::saveSnapshot()
{
    m_snapshot.map = getCurrentMap();
}

void MapFrontend::restoreSnapshot()
{
    setCurrentMap(m_snapshot.map);
}

void MapFrontend::setCurrentMap(const MapApplyResult &result)
{
    // NOTE: This is very important: it's where the map is actually changed!
    m_current.map = result.map;
    const auto roomUpdateFlags = result.roomUpdateFlags;

    this->RoomModificationTracker::notifyModified(roomUpdateFlags);
    // TODO: move checkSize() into the notifyModified().
    if (roomUpdateFlags.contains(RoomUpdateEnum::BoundsChanged)) {
        checkSize();
    }
}

void MapFrontend::setCurrentMap(Map map)
{
    // Always update everything when the map is changed like this.
    setCurrentMap(MapApplyResult{std::move(map)});

    // Chunks will be marked as dirty by the consumer (e.g. MapCanvas)
    // if it needs a full rebuild.
    m_dirtyChunks.clear();

    m_history.clearAll();
    emitUndoRedoAvailability();
}

bool MapFrontend::applyChangesInternal(
    ProgressCounter &pc,
    const std::function<MapApplyResult(Map &, ProgressCounter &)> &applyFunction)
{
    Map previous = m_current.map;

    MapApplyResult result;
    try {
        result = applyFunction(m_current.map, pc);
    } catch (const std::exception &e) {
        MMLOG_ERROR() << "Exception: " << e.what();
        global::sendToUser(QString("%1\n").arg(e.what()));
        return false;
    }

    setCurrentMap(result);
    if (m_current.map != previous) {
        m_history.recordChange(previous);
    } else {
        m_history.redo_stack.clear();
    }
    emitUndoRedoAvailability();
    return true;
}

namespace {
struct DirtyChunkVisitor
{
    const Map &oldMap;
    const Map &newMap;
    std::set<ChunkId> &dirty;

    void mark(RoomId id)
    {
        if (id == INVALID_ROOMID)
            return;
        if (auto h = oldMap.findRoomHandle(id))
            dirty.insert(ChunkId::fromCoordinate(h.getPosition()));
        if (auto h = newMap.findRoomHandle(id))
            dirty.insert(ChunkId::fromCoordinate(h.getPosition()));
    }

    void operator()(const world_change_types::CompactRoomIds &)
    {
        newMap.getRooms().for_each([&](const RoomId id) {
            dirty.insert(ChunkId::fromCoordinate(newMap.getRoomHandle(id).getPosition()));
        });
    }
    void operator()(const world_change_types::RemoveAllDoorNames &)
    {
        newMap.getRooms().for_each([&](const RoomId id) {
            dirty.insert(ChunkId::fromCoordinate(newMap.getRoomHandle(id).getPosition()));
        });
    }
    void operator()(const world_change_types::GenerateBaseMap &)
    {
        newMap.getRooms().for_each([&](const RoomId id) {
            dirty.insert(ChunkId::fromCoordinate(newMap.getRoomHandle(id).getPosition()));
        });
    }

    void operator()(const room_change_types::AddPermanentRoom &c)
    {
        dirty.insert(ChunkId::fromCoordinate(c.position));
    }
    void operator()(const room_change_types::AddRoom2 &c)
    {
        dirty.insert(ChunkId::fromCoordinate(c.position));
    }
    void operator()(const room_change_types::RemoveRoom &c) { mark(c.room); }
    void operator()(const room_change_types::UndeleteRoom &c)
    {
        dirty.insert(ChunkId::fromCoordinate(c.raw.position));
    }
    void operator()(const room_change_types::MakePermanent &c) { mark(c.room); }
    void operator()(const room_change_types::Update &c) { mark(c.room); }
    void operator()(const room_change_types::SetServerId &c) { mark(c.room); }
    void operator()(const room_change_types::MoveRelative &c) { mark(c.room); }
    void operator()(const room_change_types::MoveRelative2 &c)
    {
        for (const RoomId id : c.rooms)
            mark(id);
    }
    void operator()(const room_change_types::MergeRelative &c) { mark(c.room); }
    void operator()(const room_change_types::ModifyRoomFlags &c) { mark(c.room); }
    void operator()(const room_change_types::TryMoveCloseTo &c) { mark(c.room); }

    void operator()(const exit_change_types::ModifyExitConnection &c)
    {
        mark(c.room);
        mark(c.to);
    }
    void operator()(const exit_change_types::ModifyExitFlags &c) { mark(c.room); }
    void operator()(const exit_change_types::NukeExit &c) { mark(c.room); }
    void operator()(const exit_change_types::SetExitFlags &c) { mark(c.room); }
    void operator()(const exit_change_types::SetDoorFlags &c) { mark(c.room); }
    void operator()(const exit_change_types::SetDoorName &c) { mark(c.room); }

    void operator()(const infomark_change_types::AddInfomark &) {}
    void operator()(const infomark_change_types::UpdateInfomark &) {}
    void operator()(const infomark_change_types::RemoveInfomark &) {}
};
} // namespace

bool MapFrontend::applySingleChange(ProgressCounter &pc, const Change &change)
{
    if (IS_DEBUG_BUILD) {
        auto &&log = MMLOG();
        log << "[MapFrontend::applySingleChange] ";
        getCurrentMap().printChange(log, change);
    }
    Map prev = m_current.map;
    bool success = applyChangesInternal(pc, [&](Map &map, ProgressCounter &counter) {
        return map.applySingleChange(counter, change);
    });
    if (success) {
        change.acceptVisitor(DirtyChunkVisitor{prev, m_current.map, m_dirtyChunks});
    }
    return success;
}

bool MapFrontend::applySingleChange(const Change &change)
{
    ProgressCounter dummyPc;
    return applySingleChange(dummyPc, change);
}

bool MapFrontend::applyChanges(ProgressCounter &pc, const ChangeList &changes)
{
    if (IS_DEBUG_BUILD) {
        auto &&log = MMLOG();
        log << "[MapFrontend::applyChanges] ";
        getCurrentMap().printChanges(log, changes.getChanges(), "\n");
    }
    Map prev = m_current.map;
    bool success = applyChangesInternal(pc, [&](Map &map, ProgressCounter &counter) {
        return map.apply(counter, changes);
    });
    if (success) {
        DirtyChunkVisitor visitor{prev, m_current.map, m_dirtyChunks};
        for (const auto &change : changes.getChanges()) {
            change.acceptVisitor(visitor);
        }
    }
    return success;
}

bool MapFrontend::applyChanges(const ChangeList &changes)
{
    ProgressCounter dummyPc;
    return applyChanges(dummyPc, changes);
}

void MapFrontend::emitUndoRedoAvailability()
{
    emit sig_undoAvailable(m_history.isUndoAvailable());
    emit sig_redoAvailable(m_history.isRedoAvailable());
}

void MapFrontend::slot_undo()
{
    if (std::optional<Map> optMap = m_history.undo(m_current.map)) {
        setCurrentMap(MapApplyResult{std::move(*optMap)});
    }
    emitUndoRedoAvailability();
}

void MapFrontend::slot_redo()
{
    if (std::optional<Map> optMap = m_history.redo(m_current.map)) {
        setCurrentMap(MapApplyResult{std::move(*optMap)});
    }
    emitUndoRedoAvailability();
}
