#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "../global/ImmUnorderedSet.h"
#include "../map/roomid.h"

#include <cstdint>
#include <functional>
#include <memory>

#include <QString>

class GmcpMessage;
class KnownRoomsStore;

// Per-connection, per-character state.
//
// The snapshot holds every room the character is known to have discovered so far: the cached
// history, the chunks of the server's list as they arrive, and live Room.Known.Add messages.
// It is trusted for additions at all times. Only a terminated Room.Known.List is
// authoritative, so only it can remove stale IDs. Consumers must treat anything not in the
// snapshot as unknown (fail closed); there is deliberately no "undiscovered" predicate that
// stays false until a list completes.
// Persistent sets give the renderer cheap immutable snapshots without copying all room IDs.
class NODISCARD KnownRooms final
{
public:
    struct NODISCARD Snapshot final
    {
        ImmUnorderedSet<ServerRoomId> rooms;
        uint64_t revision = 0;
        uint64_t generation = 0;
        // True once a terminated server list has replaced any provisional (cached) history.
        // Not a visibility gate: use it for status ("updating discoveries") only.
        bool listComplete = false;

        NODISCARD bool isKnown(const ServerRoomId id) const
        {
            return id != INVALID_SERVER_ROOMID && rooms.contains(id);
        }
    };

private:
    Snapshot m_snapshot;
    ImmUnorderedSet<ServerRoomId> m_liveKnown;
    bool m_authoritative = false;
    ImmUnorderedSet<ServerRoomId> m_pending;
    QString m_character;
    bool m_enabled = false;
    bool m_receiving = false;
    bool m_discard = false;
    bool m_refresh = false;

    std::unique_ptr<KnownRoomsStore> m_cache;
    std::function<void()> m_cacheChanged;
    std::function<void()> m_cacheMissing;
    bool m_cacheWarningIssued = false;
    // The saved history for the current character was found and restored.
    bool m_cacheHit = false;
    void loadCache();
    void saveCache(bool immediate = false);
    void restoreCache(const QString &character,
                      uint64_t generation,
                      const ImmUnorderedSet<ServerRoomId> &rooms);
    void invalidate();
    NODISCARD bool requestList();

public:
    KnownRooms();
    ~KnownRooms();
    void configureCache(const QString &directory,
                        const QString &server,
                        std::function<void()> changed,
                        std::function<void()> missing = {});
    NODISCARD const Snapshot &getSnapshot() const { return m_snapshot; }
    // The identified character; empty until GMCP names one.
    NODISCARD const QString &getCharacter() const { return m_character; }
    void reset();
    // True means the caller must send one Room.Known.List request.
    NODISCARD bool setEnabled(bool enabled);
    NODISCARD bool receive(const GmcpMessage &message);
};
