#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2024 The MMapper Authors

#include "../global/macros.h"
#include "room.h" // Includes RoomId, ServerRoomId, INVALID_ROOMID, INVALID_SERVER_ROOMID
#include <immer/map.hpp>
#include <optional> // For std::optional

class AnsiOstream;
class ProgressCounter;

struct NODISCARD ServerIdMap final
{
private:
    immer::map<ServerRoomId, RoomId> m_serverToInternal;

public:
    NODISCARD bool empty() const { return m_serverToInternal.empty(); }
    NODISCARD size_t size() const { return m_serverToInternal.size(); } // Explicitly size_t

    NODISCARD bool contains(const ServerRoomId serverId) const
    {
        return m_serverToInternal.count(serverId) > 0;
    }

    NODISCARD std::optional<RoomId> lookup(const ServerRoomId serverId) const
    {
        const RoomId* ptr = m_serverToInternal.find(serverId);
        if (ptr) {
            return *ptr;
        }
        return std::nullopt;
    }

    void set(const ServerRoomId serverId, const RoomId id)
    {
        if (serverId != INVALID_SERVER_ROOMID && id != INVALID_ROOMID) {
            m_serverToInternal = m_serverToInternal.set(serverId, id);
        }
    }

    void remove(const ServerRoomId serverId)
    {
        if (serverId != INVALID_SERVER_ROOMID) {
            // Check if key exists before erase to maintain exact previous behavior
            // (OrderedMap::erase might not have cared if key exists)
            // immer::map::erase on non-existent key is a no-op and returns same map.
            m_serverToInternal = m_serverToInternal.erase(serverId);
        }
    }

    // Callback = void(ServerRoomId, RoomId);
    template<typename Callback>
    void for_each(Callback &&callback) const
    {
        static_assert(std::is_invocable_r_v<void, Callback, ServerRoomId, RoomId>);
        for (const auto &kv_pair : m_serverToInternal) { // Iterates std::pair<const Key, Value>
            callback(kv_pair.first, kv_pair.second);
        }
    }

    void printStats(ProgressCounter &pc, AnsiOstream &os) const;

    // immer::map has its own operator==
    NODISCARD bool operator==(const ServerIdMap &rhs) const
    {
        return m_serverToInternal == rhs.m_serverToInternal;
    }
    NODISCARD bool operator!=(const ServerIdMap &rhs) const { return !(rhs == *this); }
};
