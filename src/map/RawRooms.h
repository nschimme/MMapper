#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2024 The MMapper Authors

#include "../global/macros.h"
#include "InvalidMapOperation.h"
#include "RawExit.h"
#include "RawRoom.h"
#include "roomid.h" // Ensure RoomId is available

#include <immer/vector.hpp>
#include <immer/vector_transient.hpp> // For immer::vector::transient_type
#include <stdexcept> // For std::out_of_range

class NODISCARD RawRooms final
{
private:
    immer::vector<RawRoom> m_rooms;

    // Helper to check bounds and convert RoomId to size_t
    NODISCARD size_t toIndex(RoomId id) const {
        const auto index = id.asUint32();
        if (index >= m_rooms.size()) {
            throw std::out_of_range("RoomId out of bounds");
        }
        return index;
    }

public:
    // Get by const reference
    NODISCARD const RawRoom &getRawRoomRef(RoomId pos) const { return m_rooms[toIndex(pos)]; }

    // Setters will now update the persistent vector and reassign m_rooms
    // This is a general pattern for setters. Individual setters below will use this.
    void updateRoom(RoomId id, std::function<RawRoom(RawRoom)> updater) {
        const auto index = toIndex(id);
        m_rooms = m_rooms.set(index, updater(m_rooms[index]));
    }

public:
    NODISCARD size_t size() const { return m_rooms.size(); }
    void resize(const size_t numRooms) {
        // immer::vector can be resized by taking a subvector or appending.
        // If shrinking, take a subvector. If growing, append default RawRooms.
        if (numRooms < m_rooms.size()) {
            m_rooms = m_rooms.take(numRooms);
        } else if (numRooms > m_rooms.size()) {
            auto new_rooms = m_rooms.transient();
            for(size_t i = m_rooms.size(); i < numRooms; ++i) {
                new_rooms.push_back(RawRoom{});
            }
            m_rooms = new_rooms.persistent();
        }
    }

    void removeAt(const RoomId id) {
        // Set to default RawRoom, consistent with previous logic
        updateRoom(id, [](RawRoom /*old_room*/){ return RawRoom{}; });
    }

    void requireUninitialized(const RoomId id) const
    {
        if (getRawRoomRef(id) != RawRoom{}) { // Assuming RawRoom has operator!=
            throw InvalidMapOperation();
        }
    }

public:
#define X_DECL_ACCESSORS(_Type, _Name, _Init) \
    NODISCARD const _Type &getRoom##_Name(const RoomId id) const \
    { \
        return getRawRoomRef(id).fields._Name; \
    } \
    void setRoom##_Name(const RoomId id, _Type x) \
    { \
        if (getRoom##_Name(id) != x) { \
            updateRoom(id, [val = std::move(x)](RawRoom room) { \
                room.fields._Name = val; \
                return room; \
            }); \
        } \
    }

    XFOREACH_ROOM_STRING_PROPERTY(X_DECL_ACCESSORS)
    XFOREACH_ROOM_FLAG_PROPERTY(X_DECL_ACCESSORS)
    XFOREACH_ROOM_ENUM_PROPERTY(X_DECL_ACCESSORS)
#undef X_DECL_ACCESSORS

public:
#define X_DEFINE_ACCESSOR(_Type, _Name, _Init) \
    void setExit##_Type(const RoomId id, const ExitDirEnum dir, _Type x) \
    { \
        if (getExit##_Type(id, dir) != x) { \
            updateRoom(id, [dir, val = std::move(x)](RawRoom room) { \
                room.getExit(dir).fields._Name = val; \
                return room; \
            }); \
        } \
    } \
    NODISCARD const _Type &getExit##_Type(const RoomId id, const ExitDirEnum dir) const \
    { \
        return getRawRoomRef(id).getExit(dir).fields._Name; \
    }
    XFOREACH_EXIT_PROPERTY(X_DEFINE_ACCESSOR)
#undef X_DEFINE_ACCESSOR

public:
    void setExitOutgoing(const RoomId id, const ExitDirEnum dir, const TinyRoomIdSet &set)
    {
        updateRoom(id, [this, id, dir, &set](RawRoom room) { // Capturing 'this' and 'id' if enforceInvariants needs them
            room.getExit(dir).outgoing = set;
            // Call the member function enforceInvariants which should operate on the provided 'room'
            // This requires RawRooms::enforceInvariants to be callable in a way that can modify 'room'
            // or for ::enforceInvariants to be called directly on room.getExit(dir)
            ::enforceInvariants(room.getExit(dir)); // Assuming global ::enforceInvariants is desired
            return room;
        });
    }
    NODISCARD const TinyRoomIdSet &getExitOutgoing(const RoomId id, const ExitDirEnum dir) const
    {
        return getRawRoomRef(id).getExit(dir).outgoing;
    }

public:
    void setExitIncoming(const RoomId id, const ExitDirEnum dir, const TinyRoomIdSet &set)
    {
        updateRoom(id, [dir, &set](RawRoom room) {
            room.getExit(dir).incoming = set;
            return room;
        });
    }
    NODISCARD const TinyRoomIdSet &getExitIncoming(const RoomId id, const ExitDirEnum dir) const
    {
        return getRawRoomRef(id).getExit(dir).incoming;
    }

public:
    void setExitFlags_safe(RoomId id, ExitDirEnum dir, ExitFlags input_flags)
    {
        updateRoom(id, [dir, input_flags](RawRoom room) {
            room.getExit(dir).fields.exitFlags = input_flags; // Corrected .flags to .exitFlags
            ::enforceInvariants(room.getExit(dir));
            return room;
        });
    }

public:
    // These now primarily serve satisfiesInvariants or internal logic,
    // as direct modification is within updateRoom lambdas.
    // If they were intended for users to call to fixup state, that pattern changes.
    void enforceInvariants(RoomId id, ExitDirEnum dir); // Implementation might need review
    void enforceInvariants(RoomId id);

public:
    NODISCARD bool satisfiesInvariants(RoomId id, ExitDirEnum dir) const;
    NODISCARD bool satisfiesInvariants(RoomId id) const;

public:
    NODISCARD ExitFlags getExitFlags(const RoomId id, const ExitDirEnum dir) const
    {
        return getExitExitFlags(id, dir);
    }
    void setExitInOut(const RoomId id,
                      const ExitDirEnum dir,
                      const InOutEnum inOut,
                      const TinyRoomIdSet &set)
    {
        const bool isOut = inOut == InOutEnum::Out;
        updateRoom(id, [this, id, dir, isOut, &set](RawRoom room){ // Capturing 'this' and 'id' if needed
            auto &exit_data = room.getExit(dir);
            (isOut ? exit_data.outgoing : exit_data.incoming) = set;
            if (isOut) {
                // Call the member function enforceInvariants or global ::enforceInvariants
                ::enforceInvariants(room.getExit(dir));
            }
            return room;
        });
    }

    NODISCARD const TinyRoomIdSet &getExitInOut(const RoomId id,
                                                const ExitDirEnum dir,
                                                const InOutEnum inOut) const
    {
        const bool isOut = inOut == InOutEnum::Out;
        const auto &data = getRawRoomRef(id).getExit(dir);
        return isOut ? data.outgoing : data.incoming;
    }

public:
    void setServerId(const RoomId id, const ServerRoomId serverId)
    {
        updateRoom(id, [serverId](RawRoom room){
            room.server_id = serverId;
            return room;
        });
    }
    NODISCARD const ServerRoomId &getServerId(const RoomId id) const
    {
        return getRawRoomRef(id).server_id;
    }

public:
    void setPosition(const RoomId id, const Coordinate &coord)
    {
        updateRoom(id, [&coord](RawRoom room){
            room.position = coord;
            return room;
        });
    }
    NODISCARD const Coordinate &getPosition(const RoomId id) const
    {
        return getRawRoomRef(id).position;
    }

public:
    NODISCARD RoomStatusEnum getStatus(const RoomId id) const
    {
        return getRawRoomRef(id).status;
    }
    void setStatus(const RoomId id, RoomStatusEnum status)
    {
        if (status != getStatus(id)) {
             updateRoom(id, [status](RawRoom room){
                room.status = status;
                return room;
            });
        }
    }

public:
    // immer::vector has its own operator==
    NODISCARD bool operator==(const RawRooms &rhs) const
    {
        return m_rooms == rhs.m_rooms;
    }
    NODISCARD bool operator!=(const RawRooms &rhs) const
    {
        return !(rhs == *this);
    }
};
