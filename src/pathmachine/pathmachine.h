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
#include "../map/roomid.h" // For RoomIdSet
#include "path.h"
#include "pathparameters.h"

#include <map>
#include <memory>
#include <optional>
#include <set>

#include <QString>
#include <QtCore>

class Approved;
class Coordinate;
class MapFrontend;
class QEvent;
class QObject;
struct RoomId;

enum class NODISCARD PathStateEnum : uint8_t { APPROVED = 0, EXPERIMENTING = 1, SYNCING = 2 };

class PathProcessor {
public:
    virtual ~PathProcessor() = default;
    virtual void processRoom(const RoomHandle &room, const ParseEvent &event) = 0;
};

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
    SigParseEvent m_lastEvent;
    std::shared_ptr<PathList> m_paths;
    std::optional<RoomId> m_pathRoot;
    std::optional<RoomId> m_mostLikelyRoom;
    PathStateEnum m_state = PathStateEnum::SYNCING;

    // Members from RoomSignalHandler
    RoomIdSet m_roomOwners;
    std::map<RoomId, std::set<PathProcessor*>> m_roomLockers;
    std::map<RoomId, int> m_roomHoldCount;

public:
    void onPositionChange(std::optional<RoomId> optId)
    {
        forcePositionChange(optId.value_or(INVALID_ROOMID), false);
    }
    void forceUpdate(const RoomId id) { forcePositionChange(id, true); }
    NODISCARD bool hasLastEvent() const;
    NODISCARD size_t getNumLockers(RoomId room) const;

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
    void tryExits(const RoomHandle &, PathProcessor &, const ParseEvent &, bool out);
    void tryExit(const RawExit &possible, PathProcessor &processor, bool out);
    void tryCoordinate(const RoomHandle &, PathProcessor &, const ParseEvent &);

    // Methods from RoomSignalHandler
    void holdRoom(RoomId room, PathProcessor *processor);
    void releaseRoom(RoomId room, ChangeList &changes);
    void keepRoom(RoomId room, ExitDirEnum dir, RoomId fromId, ChangeList &changes);

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
