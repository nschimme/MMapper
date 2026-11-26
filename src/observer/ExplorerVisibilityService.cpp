// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "ExplorerVisibilityService.h"

#include "../map/RoomHandle.h"

namespace {
constexpr std::chrono::milliseconds COALESCE_DELAY{20};
constexpr std::chrono::milliseconds POLL_DELAY{25};
} // namespace

ExplorerVisibilityService::ExplorerVisibilityService(QObject *const parent)
    : QObject(parent)
{
    m_timer.setSingleShot(true);
    QObject::connect(&m_timer, &QTimer::timeout, &m_timer, [this]() { poll(); });
}

ExplorerVisibilityService::~ExplorerVisibilityService()
{
    m_timer.stop();
    if (m_future) {
        // Only at teardown: destroying a pending std::async future would block anyway.
        try {
            m_future->wait();
        } catch (...) {
        }
    }
}

void ExplorerVisibilityService::rebuildView()
{
    if (m_active) {
        m_view = ExplorerView::makeActive(m_base, m_map, m_current, m_exits);
    } else {
        m_view = ExplorerView{};
    }
    emit sig_changed();
}

void ExplorerVisibilityService::resetBase()
{
    m_base.reset();
    m_publishedRevision = 0;
    ++m_epoch;
}

void ExplorerVisibilityService::inputsChanged()
{
    ++m_inputRevision;
    if (m_active && !m_timer.isActive()) {
        m_timer.start(COALESCE_DELAY);
    }
}

void ExplorerVisibilityService::launch()
{
    ++m_computeCount;
    ExplorerVisibilityInput in;
    in.map = m_map;
    in.known = m_known;
    in.discoveredExits = m_exits;
    in.currentRoom = std::nullopt; // applied as an overlay by ExplorerView
    in.placedRooms = m_placed;
    const uint64_t revision = m_inputRevision;
    const uint64_t epoch = m_epoch;
    m_future = std::async(std::launch::async,
                          [input = std::move(in), revision, epoch]() -> Computed {
                              return Computed{epoch,
                                              revision,
                                              std::make_shared<const ExplorerVisibility>(
                                                  computeExplorerVisibility(input))};
                          });
}

void ExplorerVisibilityService::poll()
{
    if (m_future) {
        // Never block the GUI thread on the worker.
        if (m_future->wait_for(std::chrono::milliseconds(0)) != std::future_status::ready) {
            m_timer.start(POLL_DELAY);
            return;
        }
        std::optional<Computed> result;
        try {
            result = m_future->get();
        } catch (const std::exception &) {
            // Fail closed: keep the previous base and try again on the next change.
        }
        m_future.reset();
        // A result for inputs that have merely changed since is still safe to show: rooms it
        // does not know about are hidden. One from an older epoch (another character or map) or
        // computed while inactive is dropped.
        if (result && m_active && result->epoch == m_epoch
            && result->revision >= m_publishedRevision) {
            m_publishedRevision = result->revision;
            m_base = std::move(result->base);
            rebuildView();
        }
    }
    if (m_active && m_publishedRevision < m_inputRevision && !m_future) {
        launch();
        m_timer.start(POLL_DELAY);
    }
}

void ExplorerVisibilityService::setActive(const bool active)
{
    if (m_active == active) {
        return;
    }
    m_active = active;
    resetBase(); // a result still in flight is dropped when it arrives (see poll())
    if (active) {
        inputsChanged();
    } else {
        m_timer.stop();
    }
    rebuildView();
}

void ExplorerVisibilityService::setMap(const Map &map)
{
    if (m_map.isSamePointer(map)) {
        return;
    }
    m_map = map;
    inputsChanged();
    rebuildView();
}

void ExplorerVisibilityService::onMapLoaded()
{
    m_placed = RoomIdSet{};
    resetBase();
    inputsChanged();
    rebuildView();
}

void ExplorerVisibilityService::setCurrentRoom(const std::optional<RoomId> room)
{
    if (m_current == room) {
        return;
    }
    m_current = room;
    if (m_active && room) {
        // A room with no server ID has no stable key: remember it for this session only.
        const RoomHandle handle = m_map.findRoomHandle(*room);
        if (handle.exists() && !handle.isTemporary()
            && handle.getServerId() == INVALID_SERVER_ROOMID && !m_placed.contains(*room)) {
            m_placed.insert(*room);
            inputsChanged();
        }
    }
    rebuildView();
}

void ExplorerVisibilityService::setKnown(const ImmUnorderedSet<ServerRoomId> &rooms,
                                         const uint64_t revision,
                                         const uint64_t generation)
{
    if (revision == m_knownRevision && generation == m_knownGeneration) {
        return;
    }
    m_known = rooms;
    m_knownRevision = revision;
    m_knownGeneration = generation;
    inputsChanged();
}

void ExplorerVisibilityService::setDiscoveredExits(const DiscoveredExits &exits,
                                                   const uint64_t revision)
{
    if (revision == m_exitsRevision) {
        return;
    }
    m_exits = exits;
    m_exitsRevision = revision;
    inputsChanged();
    rebuildView(); // the overlay depends on the finds too
}

void ExplorerVisibilityService::onCharacterChanged()
{
    m_placed = RoomIdSet{};
    resetBase();
    inputsChanged();
    rebuildView();
}

void ExplorerVisibilityService::onDisconnected()
{
    m_placed = RoomIdSet{};
    resetBase();
    inputsChanged();
    rebuildView();
}
