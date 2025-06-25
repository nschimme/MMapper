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

MapFrontend::MapFrontend(QObject *const parent)
    : QObject(parent)
{
    // Ensure undo/redo are initially unavailable
    emit sig_undoAvailable(false);
    emit sig_redoAvailable(false);
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
    // Push current state to undo stack to make clear undoable
    if (!m_current.map.getRoomSet().empty() || !m_undo_stack.empty()) {
        // Only push if there's something to clear or if there's an existing undo history
        // This avoids pushing an empty map if clear is called multiple times on an empty map initially.
        m_undo_stack.push_back(m_current.map);
        if (m_undo_stack.size() > MAX_UNDO_HISTORY) {
            m_undo_stack.erase(m_undo_stack.begin());
        }
    }

    if (!m_redo_stack.empty()) {
        m_redo_stack.clear();
        emit sig_redoAvailable(false);
    } else {
        emit sig_redoAvailable(false); // Ensure emitted
    }

    emit sig_clearingMap();
    setCurrentMap(Map{}); // This sets m_current.map to an empty map
    currentHasBeenSaved(); // This sets m_saved = m_current (which is now empty)
    checkSize(); // called for side effect of sending signal
    virt_clear();

    // After clear, undo is available if we pushed something, redo is not.
    emit sig_undoAvailable(!m_undo_stack.empty());
    // sig_redoAvailable already emitted or handled
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
    for (const RoomId id : map.getRooms()) {
        const auto &r = map.getRoomHandle(id);
        if (bounds.contains(r.getPosition())) {
            result.insert(r.getId());
        }
    }
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

    // When a map is set directly (e.g., loading a new map), clear undo/redo history.
    if (!m_undo_stack.empty()) {
        m_undo_stack.clear();
    }
    if (!m_redo_stack.empty()) {
        m_redo_stack.clear();
    }
    // Emit signals after stacks are definitively cleared.
    emit sig_undoAvailable(false);
    emit sig_redoAvailable(false);
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
        m_undo_stack.push_back(previous_map_state);
        if (m_undo_stack.size() > MAX_UNDO_HISTORY) {
            // Remove the oldest state if history limit exceeded
            m_undo_stack.erase(m_undo_stack.begin());
        }
    }

    // A new action always clears the redo stack.
    bool redo_stack_was_emptied = !m_redo_stack.empty();
    if (redo_stack_was_emptied) {
        m_redo_stack.clear();
    }
    // Always emit false for redo availability after a new action completes successfully.
    emit sig_redoAvailable(false);

    // Update undo availability based on the stack's current state.
    emit sig_undoAvailable(!m_undo_stack.empty());

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
        m_undo_stack.push_back(previous_map_state);
        if (m_undo_stack.size() > MAX_UNDO_HISTORY) {
            // Remove the oldest state if history limit exceeded
            m_undo_stack.erase(m_undo_stack.begin());
        }
    }

    // A new action always clears the redo stack.
    bool redo_stack_was_emptied = !m_redo_stack.empty();
    if (redo_stack_was_emptied) {
        m_redo_stack.clear();
    }
    // Always emit false for redo availability after a new action completes successfully.
    emit sig_redoAvailable(false);

    // Update undo availability based on the stack's current state.
    emit sig_undoAvailable(!m_undo_stack.empty());

    return true;
}

bool MapFrontend::applyChanges(const ChangeList &changes)
{
    ProgressCounter dummyPc;
    return applyChanges(dummyPc, changes);
}

void MapFrontend::slot_undo()
{
    if (m_undo_stack.empty()) {
        return;
    }

    m_redo_stack.push_back(m_current.map);
    // MAX_UNDO_HISTORY applies to undo stack, redo stack can grow
    // or we can also cap redo_stack if necessary, but typically not done.

    m_current.map = m_undo_stack.back();
    m_undo_stack.pop_back();

    // Call the MapApplyResult version directly to apply the map state
    // without clearing the undo/redo stacks.
    setCurrentMap(MapApplyResult{m_current.map});

    emit sig_undoAvailable(!m_undo_stack.empty());
    emit sig_redoAvailable(true); // Redo is now possible
}

void MapFrontend::slot_redo()
{
    if (m_redo_stack.empty()) {
        return;
    }

    m_undo_stack.push_back(m_current.map);
    if (m_undo_stack.size() > MAX_UNDO_HISTORY) {
        m_undo_stack.erase(m_undo_stack.begin());
    }

    m_current.map = m_redo_stack.back();
    m_redo_stack.pop_back();

    // Call the MapApplyResult version directly to apply the map state
    // without clearing the undo/redo stacks.
    setCurrentMap(MapApplyResult{m_current.map});

    emit sig_undoAvailable(true); // Undo is now possible (to undo the redo)
    emit sig_redoAvailable(!m_redo_stack.empty()); // Update redo availability
}
