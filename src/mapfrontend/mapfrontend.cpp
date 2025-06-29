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

// Define MAX_UNDO_HISTORY for initializing m_undo_history
// This could also be a private static const member in MapFrontend.h if preferred,
// but since it's only used for initialization here, a static const in the .cpp is also fine.
static const size_t MAX_UNDO_HISTORY_CONST = 100;

MapFrontend::MapFrontend(QObject *const parent)
    : QObject(parent)
    , m_history_coordinator(MAX_UNDO_HISTORY_CONST) // Initialize the coordinator
{
    // Ensure undo/redo are initially unavailable
    emit sig_undoAvailable(m_history_coordinator.isUndoAvailable()); // should be false initially
    emit sig_redoAvailable(m_history_coordinator.isRedoAvailable()); // should be false initially
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
    emit sig_clearingMap();
    setCurrentMap(m_saved.map);
}

void MapFrontend::clear()
{
    // If the current map is not empty, or if there's already an undo history,
    // then the current state should be pushed to allow undoing the clear operation.
    if (!m_current.map.empty() || m_history_coordinator.isUndoAvailable()) {
        m_history_coordinator.recordChange(m_current.map);
        // recordChange automatically clears redo and pushes to undo.
    } else {
        // recordChange automatically clears redo and pushes to undo.
    }
    // If recordChange was called, redo is cleared.
    // If recordChange was not called (map was empty and no undo history),
    // redo should already be empty from prior operations.
    // Thus, no explicit redo clear seems necessary here beyond what recordChange does.

    // Perform the actual map clearing
    emit sig_clearingMap();

    m_current.map = Map{}; // Directly set to empty map.

    // Notify about the change
    this->RoomModificationTracker::notifyModified(RoomUpdateFlags{RoomUpdateEnum::All});
    const auto new_bounds = m_current.map.getBounds().value_or(Bounds{});
    emit sig_mapSizeChanged(new_bounds.min, new_bounds.max);

    currentHasBeenSaved(); // m_saved = m_current (empty)
    virt_clear();

    // Update availability signals
    emit sig_undoAvailable(m_history_coordinator.isUndoAvailable());
    emit sig_redoAvailable(m_history_coordinator.isRedoAvailable()); // Should be false
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
    // This first call updates m_current.map and emits modification signals.
    setCurrentMap(MapApplyResult{std::move(map)});

    // When a map is set directly (e.g., loading a new map or restoring snapshot),
    // clear all undo/redo history.
    m_history_coordinator.clearAll();

    // Emit signals after stacks are definitively cleared.
    emit sig_undoAvailable(m_history_coordinator.isUndoAvailable()); // Will be false
    emit sig_redoAvailable(m_history_coordinator.isRedoAvailable()); // Will be false
}

bool MapFrontend::applySingleChange(ProgressCounter &pc, const Change &change)
{
    MapApplyResult result;
    try {
        result = m_current.map.applySingleChange(pc, change);
    } catch (const std::exception &e) {
        MMLOG_ERROR() << "Exception: " << e.what();
        global::sendToUser(QString("%1\n").arg(e.what()));
        return false;
    }

    // The state *before* this change is m_current.map. Store it.
    Map previous_map_state = m_current.map;

    // Apply the change. setCurrentMap updates m_current.map to result.map.
    setCurrentMap(result);

    // Check if the map content actually changed.
    // World::operator== is used by Map::operator==
    bool map_content_changed = m_current.map != previous_map_state;

    if (map_content_changed) {
        m_history_coordinator.recordChange(previous_map_state);
        // recordChange handles pushing to undo and clearing redo.
    } else {
        // If map content didn't change, but an action was applied,
        // it implies no change to undo history, but redo should still be cleared
        // as a new "conceptual" action occurred.
        // The recordChange method in UndoRedoCoordinator handles redo clearing.
        // If map_content_changed is false, recordChange is not called.
        // We need to ensure redo is cleared if an action is processed, even if it's a no-op.
        // This was previously handled by:
        //   bool redo_stack_was_emptied = !m_redo_stack.empty();
        //   if (redo_stack_was_emptied) { m_redo_stack.clear(); }
        //   emit sig_redoAvailable(false);
        // The UndoRedoCoordinator::recordChange already clears redo.
        // If recordChange is not called, redo is not cleared by it.
        // So, if map content did not change, we might need to explicitly clear redo.
        if (!m_history_coordinator.redo_stack.isEmpty()) {
             m_history_coordinator.redo_stack.clear();
        }
    }

    // Update availability signals
    emit sig_undoAvailable(m_history_coordinator.isUndoAvailable());
    // recordChange (if called) or the explicit clear above ensures redo is not available.
    emit sig_redoAvailable(m_history_coordinator.isRedoAvailable());

    return true;
}

bool MapFrontend::applySingleChange(const Change &change)
{
    if (IS_DEBUG_BUILD) {
        auto &&log = MMLOG();
        log << "[MapFrontend::applySingleChange] ";
        getCurrentMap().printChange(log, change);
    }

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

    MapApplyResult result;
    try {
        result = m_current.map.apply(pc, changes);
    } catch (const std::exception &e) {
        MMLOG_ERROR() << "Exception: " << e.what();
        global::sendToUser(QString("%1\n").arg(e.what()));
        return false;
    }

    // The state *before* this batch of changes is m_current.map. Store it.
    Map previous_map_state = m_current.map;

    // Apply the changes. setCurrentMap updates m_current.map to result.map.
    setCurrentMap(result);

    // Check if the map content actually changed.
    bool map_content_changed = m_current.map != previous_map_state;

    if (map_content_changed) {
        m_history_coordinator.recordChange(previous_map_state);
        // recordChange handles pushing to undo and clearing redo.
    } else {
        // If map content didn't change, but an action was applied,
        // redo should still be cleared.
        if (!m_history_coordinator.redo_stack.isEmpty()) {
             m_history_coordinator.redo_stack.clear();
        }
    }

    // Update availability signals
    emit sig_undoAvailable(m_history_coordinator.isUndoAvailable());
    // recordChange (if called) or the explicit clear above ensures redo is not available.
    emit sig_redoAvailable(m_history_coordinator.isRedoAvailable());

    return true;
}

bool MapFrontend::applyChanges(const ChangeList &changes)
{
    ProgressCounter dummyPc;
    return applyChanges(dummyPc, changes);
}

void MapFrontend::slot_undo()
{
    std::optional<Map> undone_state = m_history_coordinator.undo(m_current.map);
    if (undone_state)
    {
        m_current.map = std::move(*undone_state);
        // Apply the map state without clearing history stacks (handled by MapApplyResult version)
        setCurrentMap(MapApplyResult{m_current.map});
    }
    // Update availability signals
    emit sig_undoAvailable(m_history_coordinator.isUndoAvailable());
    emit sig_redoAvailable(m_history_coordinator.isRedoAvailable());
}

void MapFrontend::slot_redo()
{
    std::optional<Map> redone_state = m_history_coordinator.redo(m_current.map);
    if (redone_state)
    {
        m_current.map = std::move(*redone_state);
        // Apply the map state without clearing history stacks
        setCurrentMap(MapApplyResult{m_current.map});
    }
    // Update availability signals
    emit sig_undoAvailable(m_history_coordinator.isUndoAvailable());
    emit sig_redoAvailable(m_history_coordinator.isRedoAvailable());
}
