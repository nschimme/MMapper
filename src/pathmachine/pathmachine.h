#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Ulf Hermann <ulfonk_mennhar@gmx.de> (Alve)
// Author: Marek Krejza <krejza@gmail.com> (Caligor)
// Author: Nils Schimmelmann <nschimme@gmail.com> (Jahara)

#include "../configuration/configuration.h"
#include "../map/ChangeList.h"
#include "../map/parseevent.h"
#include "../map/room.h"
#include "path.h"
#include "pathparameters.h"
// #include "roomsignalhandler.h" // Removed include

#include <memory>
#include <optional>

#include <QString>
#include <QtCore>

class Approved;
class Coordinate;
class MapFrontend;
class QEvent;
class QObject;
// class RoomRecipient; // Forward declaration removed
struct RoomId;

enum class NODISCARD PathStateEnum : uint8_t { APPROVED = 0, EXPERIMENTING = 1, SYNCING = 2 };

/**
 * the parser determines the relations between incoming move- and room-events
 * and decides if rooms have to be added (and where) and where the player is
 * the results are published via signals
 *
 * PathMachine is the base class for Mmapper2PathMachine
 */
class NODISCARD_QOBJECT PathMachine : public QObject
{
    Q_OBJECT

protected:
    PathParameters m_params;

private:
    MapFrontend &m_map;
    // RoomSignalHandler m_signaler; // Removed member
    SigParseEvent m_lastEvent;
    std::shared_ptr<PathList> m_paths;
    std::optional<RoomId> m_pathRoot;
    std::optional<RoomId> m_mostLikelyRoom;
    PathStateEnum m_state = PathStateEnum::SYNCING;

public:
    void onPositionChange(std::optional<RoomId> optId)
    {
        forcePositionChange(optId.value_or(INVALID_ROOMID), false);
    }
    void forceUpdate(const RoomId id) { forcePositionChange(id, true); }
    NODISCARD bool hasLastEvent() const;

public:
    void onMapLoaded();

protected:
    explicit PathMachine(MapFrontend &map, QObject *parent);

protected:
    void handleParseEvent(const SigParseEvent &);

private:
    void scheduleAction(const ChangeList &action);
    void forcePositionChange(RoomId id, bool update);

private:
    void experimenting(const SigParseEvent &sigParseEvent, ChangeList &changes);
    void syncing(const SigParseEvent &sigParseEvent, ChangeList &changes);
    void approved(const SigParseEvent &sigParseEvent, ChangeList &changes);
    void evaluatePaths(ChangeList &changes);
    // tryExits, tryExit, tryCoordinate declarations removed, will be defined as templates below

private:
    void updateMostLikelyRoom(const SigParseEvent &sigParseEvent, ChangeList &changes, bool force);

private:
    void clearMostLikelyRoom() { m_mostLikelyRoom.reset(); }
    void setMostLikelyRoom(RoomId roomId);

protected:
    NODISCARD PathStateEnum getState() const { return m_state; }
    NODISCARD MapModeEnum getMapMode() const { return getConfig().general.mapMode; }

private:
    NODISCARD bool hasMostLikelyRoom() const { return m_mostLikelyRoom.has_value(); }
    // NOTE: This can fail.
    NODISCARD std::optional<Coordinate> tryGetMostLikelyRoomPosition() const;
    // NOTE: This can fail.
    NODISCARD RoomHandle getPathRoot() const;
    // NOTE: This can fail.
    NODISCARD RoomHandle getMostLikelyRoom() const;

signals:
    void sig_playerMoved(RoomId id);

public slots:
    void slot_releaseAllPaths();
};

// Placed after class definition due to template nature and length.
#include "../map/ExitDirection.h" // For getDirection, ALL_EXITS7, ::exitDir
#include "../map/coordinate.h"   // For Coordinate
#include "../map/mmapper2room.h" // For isDirection7, getDirection in ParseEvent context
#include "../map/RawExit.h"      // For RawExit type in tryExit

template <typename TProcessor>
void PathMachine::tryExits(const RoomHandle &room, TProcessor &processor, const ParseEvent &event, bool out)
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
                processor.processRoom(rh);
            }
        }
        if (move >= CommandEnum::FLEE) {
            // Only try all possible exits for commands FLEE, SCOUT, and NONE
            for (const auto &possible_exit : room.getExits()) { // Renamed to avoid conflict
                tryExit(possible_exit, processor, out);
            }
        }
    }
}

template <typename TProcessor>
void PathMachine::tryExit(const RawExit &possible, TProcessor &processor, bool out)
{
    for (auto idx : (out ? possible.getOutgoingSet() : possible.getIncomingSet())) {
        RoomIdSet ids = m_map.lookingForRooms(idx);
        for (RoomId id : ids) {
            if (auto rh = m_map.findRoomHandle(id)) {
                processor.processRoom(rh);
            }
        }
    }
}

template <typename TProcessor>
void PathMachine::tryCoordinate(const RoomHandle &room, TProcessor &processor, const ParseEvent &event)
{
    if (!room.exists()) {
        // most likely room doesn't exist
        return;
    }

    const CommandEnum moveCode = event.getMoveType();
    if (moveCode < CommandEnum::FLEE) {
        // LOOK, UNKNOWN will have an empty offset
        auto offset = ::exitDir(getDirection(moveCode)); // ::exitDir for global scope
        const Coordinate c = room.getPosition() + offset;
        RoomIdSet ids_c = m_map.lookingForRooms(c);
        for (RoomId id : ids_c) {
            if (auto rh = m_map.findRoomHandle(id)) {
                processor.processRoom(rh);
            }
        }

    } else {
        const Coordinate roomPos = room.getPosition();
        for (const ExitDirEnum dir : ALL_EXITS7) {
            RoomIdSet ids_dir = m_map.lookingForRooms(roomPos + ::exitDir(dir)); // ::exitDir for global scope
            for (RoomId id : ids_dir) {
                if (auto rh = m_map.findRoomHandle(id)) {
                    processor.processRoom(rh);
                }
            }
        }
    }
}
