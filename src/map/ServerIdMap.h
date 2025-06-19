#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2024 The MMapper Authors

#include <immer/map.hpp>
#include "../global/macros.h"
#include "room.h" // Contains ServerRoomId, RoomId

class AnsiOstream;
class ProgressCounter;

struct NODISCARD ServerIdMap final
{
private:
    immer::map<ServerRoomId, RoomId> m_serverToInternal;

public:
    NODISCARD bool empty() const { return m_serverToInternal.empty(); } // immer::map has empty()
    NODISCARD size_t size() const { return m_serverToInternal.size(); } // immer::map has size()
    NODISCARD bool contains(const ServerRoomId serverId) const
    {
        return lookup(serverId).has_value();
    }
    NODISCARD std::optional<RoomId> lookup(const ServerRoomId serverId) const
    {
        // immer::map::find returns const Value* or nullptr
        if (const RoomId *found_id = m_serverToInternal.find(serverId)) {
            return *found_id;
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
            // erase returns new map, no need to check if element existed before.
            m_serverToInternal = m_serverToInternal.erase(serverId);
        }
    }

    // Callback = void(ServerRoomId, RoomId);
    template<typename Callback>
    void for_each(Callback &&callback) const
    {
        // immer::map's for_each passes const Key&, const Value&.
        // The static_assert should still hold.
        static_assert(std::is_invocable_r_v<void, Callback, ServerRoomId, RoomId>);
        m_serverToInternal.for_each(callback);
    }

    void printStats(ProgressCounter &pc, AnsiOstream &os) const;

    NODISCARD bool operator==(const ServerIdMap &rhs) const
    {
        return m_serverToInternal == rhs.m_serverToInternal;
    }
    NODISCARD bool operator!=(const ServerIdMap &rhs) const { return !(rhs == *this); }
};
