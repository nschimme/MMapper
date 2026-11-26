#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "../global/ImmUnorderedSet.h"
#include "../global/macros.h"
#include "../map/DiscoveredExit.h"
#include "../map/ExplorerVisibility.h"
#include "../map/Map.h"
#include "../map/RoomIdSet.h"
#include "../map/roomid.h"

#include <chrono>
#include <future>
#include <memory>
#include <optional>
#include <utility>

#include <QObject>
#include <QTimer>

// Session-scoped service that turns the tracker's data and the current map into the immutable
// ExplorerView consumers query. It is fed plain values through setters (MainWindow connects the
// signals), so it knows nothing about MapData, GMCP, or rendering and is testable on its own.
//
// Cost: the full computation scans every room. It runs on a worker thread over immutable
// copies of its inputs, coalesced behind a short timer and keyed by input revisions, so an
// unchanged key never recomputes. The player's room changes on every step, so it is applied as
// a cheap overlay (see ExplorerView) and never triggers a recompute.
//
// While inactive it does no work and publishes an inactive view that shows everything.
class NODISCARD_QOBJECT ExplorerVisibilityService final : public QObject
{
    Q_OBJECT

private:
    bool m_active = false;
    Map m_map;
    std::optional<RoomId> m_current;
    ImmUnorderedSet<ServerRoomId> m_known;
    uint64_t m_knownRevision = 0;
    uint64_t m_knownGeneration = 0;
    DiscoveredExits m_exits;
    uint64_t m_exitsRevision = 0;
    // Rooms without a server ID that the path machine placed the player in this session.
    RoomIdSet m_placed;

    // Bumped whenever an input of the expensive computation changes: the cache key.
    uint64_t m_inputRevision = 0;
    uint64_t m_publishedRevision = 0;
    uint64_t m_computeCount = 0;

    struct NODISCARD Computed final
    {
        uint64_t epoch = 0;
        uint64_t revision = 0;
        std::shared_ptr<const ExplorerVisibility> base;
    };
    // Bumped whenever the base stops being valid for whoever is playing (activation, map load,
    // character change, disconnect). A result computed in an older epoch must never be shown:
    // it could belong to the previous character or map.
    uint64_t m_epoch = 0;
    std::optional<std::future<Computed>> m_future;
    std::shared_ptr<const ExplorerVisibility> m_base;
    ExplorerView m_view;
    QTimer m_timer;

    void inputsChanged();
    void resetBase();
    void rebuildView();
    void poll();
    void launch();

public:
    explicit ExplorerVisibilityService(QObject *parent);
    // Teardown may wait for a running computation; nothing else ever blocks on it.
    ~ExplorerVisibilityService() final;

public:
    void setActive(bool active);
    void setMap(const Map &map);
    // RoomIds of another map are meaningless, so a loaded map forgets placed rooms and the base.
    void onMapLoaded();
    void setCurrentRoom(std::optional<RoomId> room);
    void setKnown(const ImmUnorderedSet<ServerRoomId> &rooms,
                  uint64_t revision,
                  uint64_t generation);
    void setDiscoveredExits(const DiscoveredExits &exits, uint64_t revision);
    // Placed rooms and the shared base belong to one character's session.
    void onCharacterChanged();
    void onDisconnected();

    NODISCARD const ExplorerView &getView() const { return m_view; }
    NODISCARD const RoomIdSet &getPlacedRooms() const { return m_placed; }
    // How many full computations were started; lets tests check the cache key.
    NODISCARD uint64_t getComputeCount() const { return m_computeCount; }

signals:
    // The published view changed.
    void sig_changed();
};
