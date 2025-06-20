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
#include "roomsignalhandler.h"

#include <memory>
#include <optional>
#include <unordered_set> // Added for std::unordered_set

#include <QString>
#include <QtCore>

class Approved;
class Coordinate;
class MapFrontend;
class QEvent;
class QObject;
// class RoomRecipient; // Replaced by PathProcessor
class PathProcessor;   // Forward declaration
struct RoomId;

enum class NODISCARD PathStateEnum : uint8_t { APPROVED = 0, EXPERIMENTING = 1, SYNCING = 2 };

/**
 * @brief Orchestrates pathfinding by processing game events and managing map hypotheses.
 *
 * PathMachine determines the player's current location by interpreting parse events.
 * It maintains pathfinding state (`m_state`), manages potential paths (`m_paths`),
 * and uses PathProcessor strategies for state-specific logic.
 *
 * Key Responsibilities:
 * - State Management (`PathStateEnum::APPROVED`, `EXPERIMENTING`, `SYNCING`).
 * - Event Handling (`handleParseEvent` delegates to `approved()`, `experimenting()`, `syncing()`).
 * - PathProcessor Strategy Usage: Instantiates strategies (Approved, Syncing, etc.)
 *   as std::shared_ptr to process rooms.
 * - Path Lifecycle: Manages `m_paths` (a PathList) using `Path::fork()`, `approve()`, `deny()`.
 *   `evaluatePaths()` prunes `m_paths`.
 * - Room Data Updates: `updateMostLikelyRoom()` (with helpers) updates map data.
 * - ChangeList Management: Queues all map modifications to a `ChangeList`, then
 *   calls `scheduleAction()` to apply them via MapFrontend.
 * - RoomSignalHandler Ownership: Owns `m_signaler` to manage room holds.
 */
class NODISCARD_QOBJECT PathMachine : public QObject
{
    Q_OBJECT

protected:
    PathParameters m_params;

private:
    MapFrontend &m_map;
    RoomSignalHandler m_signaler;
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

    // New helper functions
    void findAndReceiveRooms(RoomId targetRoomId, PathProcessor &recipient, ChangeList &changes);
    void findAndReceiveRooms(const Coordinate &targetCoord, PathProcessor &recipient, ChangeList &changes);

    void tryExits(const RoomHandle &room, PathProcessor &recipient, const ParseEvent &event, bool out, ChangeList &changes);
    void tryExit(const RawExit &possible, PathProcessor &recipient, bool out, ChangeList &changes);
    void tryCoordinate(const RoomHandle &room, PathProcessor &recipient, const ParseEvent &event, ChangeList &changes);

private:
    void updateMostLikelyRoom(const SigParseEvent &sigParseEvent, ChangeList &changes, bool force);

    // Helper methods for updateMostLikelyRoom
    void helperUpdateServerId(const ParseEvent &event, const RoomHandle &here, ChangeList &changes, std::unordered_set<ServerRoomId> &addedServerIds);
    void helperProcessExitsFromServerIds(const ParseEvent &event, const RoomHandle &here, ChangeList &changes, bool force, std::unordered_set<ServerRoomId> &addedServerIds);
    void helperUpdateExitAndDoorFlags(const ParseEvent &event, const RoomHandle &here, ChangeList &changes, bool force);
    void helperUpdateRoomLight(const ParseEvent &event, const RoomHandle &here, ChangeList &changes);
    void helperUpdateAdjacentRoomSundeath(const ParseEvent &event, const RoomHandle &here, ChangeList &changes);

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
