#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2024 The MMapper Authors

#include "../global/CopyOnWrite.h" // Corrected path
#include "../global/IndexedVector.h"
#include "../global/macros.h"
#include "InvalidMapOperation.h"
#include "RawExit.h"
#include "RawRoom.h"

#include <memory> // Added

class NODISCARD RawRooms final
{
private:
    IndexedVector<mm::CopyOnWrite<RawRoom>, RoomId> m_rooms; // Changed to mm::CopyOnWrite<RawRoom>

public:
    NODISCARD mm::CopyOnWrite<RawRoom> &getRawRoomRef(RoomId pos)
    {
        return m_rooms.at(pos);
    } // Return mm::CopyOnWrite<RawRoom>&
    NODISCARD const mm::CopyOnWrite<RawRoom> &getRawRoomRef(RoomId pos) const
    {
        return m_rooms.at(pos);
    } // Return const mm::CopyOnWrite<RawRoom>&

public:
    NODISCARD size_t size() const { return m_rooms.size(); }
    void resize(const size_t numRooms) { m_rooms.resize(numRooms); }

    void removeAt(const RoomId id)
    {
        getRawRoomRef(id) = mm::CopyOnWrite<RawRoom>(std::make_shared<RawRoom>());
    } // Updated to mm::CopyOnWrite

    void requireUninitialized(const RoomId id) const
    {
        if (*getRawRoomRef(id).get() != RawRoom{}) { // No change needed here as .get() is the same
            throw InvalidMapOperation();
        }
    }

public:
// Updated X_DECL_ACCESSORS to use get() and getMutable()
#define X_DECL_ACCESSORS(_Type, _Name, _Init) \
    NODISCARD const _Type &getRoom##_Name(const RoomId id) const \
    { \
        return getRawRoomRef(id).get()->fields._Name; \
    } \
    void setRoom##_Name(const RoomId id, _Type x) \
    { \
        auto raw_room_ptr = getRawRoomRef(id).getMutable(); \
        if (raw_room_ptr->fields._Name != x) { \
            raw_room_ptr->fields._Name = std::move(x); \
        } \
    }

    XFOREACH_ROOM_STRING_PROPERTY(X_DECL_ACCESSORS)
    XFOREACH_ROOM_FLAG_PROPERTY(X_DECL_ACCESSORS)
    XFOREACH_ROOM_ENUM_PROPERTY(X_DECL_ACCESSORS)
#undef X_DECL_ACCESSORS

public:
// Updated X_DEFINE_ACCESSOR to use get() and getMutable()
#define X_DEFINE_ACCESSOR(_Type, _Name, _Init) \
    void setExit##_Type(const RoomId id, const ExitDirEnum dir, _Type x) \
    { \
        auto raw_room_ptr = getRawRoomRef(id).getMutable(); \
        if (raw_room_ptr->getExit(dir).fields._Name != x) { \
            raw_room_ptr->getExit(dir).fields._Name = std::move(x); \
        } \
    } \
    NODISCARD const _Type &getExit##_Type(const RoomId id, const ExitDirEnum dir) const \
    { \
        return getRawRoomRef(id).get()->getExit(dir).fields._Name; \
    }
    XFOREACH_EXIT_PROPERTY(X_DEFINE_ACCESSOR)
#undef X_DEFINE_ACCESSOR

public:
    void setExitOutgoing(const RoomId id, const ExitDirEnum dir, const TinyRoomIdSet &set)
    {
        auto raw_room_ptr = getRawRoomRef(id).getMutable();
        raw_room_ptr->getExit(dir).outgoing = set;
        enforceInvariants(id, dir); // This might need raw_room_ptr if it modifies the room
    }
    NODISCARD const TinyRoomIdSet &getExitOutgoing(const RoomId id, const ExitDirEnum dir) const
    {
        return getRawRoomRef(id).get()->getExit(dir).outgoing;
    }

public:
    void setExitIncoming(const RoomId id, const ExitDirEnum dir, const TinyRoomIdSet &set)
    {
        auto raw_room_ptr = getRawRoomRef(id).getMutable();
        raw_room_ptr->getExit(dir).incoming = set;
    }
    NODISCARD const TinyRoomIdSet &getExitIncoming(const RoomId id, const ExitDirEnum dir) const
    {
        return getRawRoomRef(id).get()->getExit(dir).incoming;
    }

public:
    void setExitFlags_safe(RoomId id, ExitDirEnum dir, ExitFlags input_flags);

public:
    void enforceInvariants(RoomId id, ExitDirEnum dir);
    void enforceInvariants(RoomId id);

public:
    NODISCARD bool satisfiesInvariants(RoomId id, ExitDirEnum dir) const;
    NODISCARD bool satisfiesInvariants(RoomId id) const;

public:
    NODISCARD ExitFlags getExitFlags(const RoomId id, const ExitDirEnum dir) const
    {
        return getExitExitFlags(id, dir); // This will use the updated getExitExitFlags macro
    }
    void setExitInOut(const RoomId id,
                      const ExitDirEnum dir,
                      const InOutEnum inOut,
                      const TinyRoomIdSet &set)
    {
        const bool isOut = inOut == InOutEnum::Out;
        auto raw_room_ptr = getRawRoomRef(id).getMutable();
        auto &data = raw_room_ptr->getExit(dir); // data is RawExit&
        (isOut ? data.outgoing : data.incoming) = set;
        if (isOut) {
            // enforceInvariants might need to be called on raw_room_ptr or take it as argument
            // For now, assuming it's a standalone function or uses getRawRoomRef internally
            enforceInvariants(id, dir);
        }
    }

    NODISCARD const TinyRoomIdSet &getExitInOut(const RoomId id,
                                                const ExitDirEnum dir,
                                                const InOutEnum inOut) const
    {
        const bool isOut = inOut == InOutEnum::Out;
        const auto &data = getRawRoomRef(id).get()->getExit(dir); // data is const RawExit&
        return isOut ? data.outgoing : data.incoming;
    }

public:
    void setServerId(const RoomId id, const ServerRoomId serverId)
    {
        getRawRoomRef(id).getMutable()->server_id = serverId;
    }
    NODISCARD const ServerRoomId &getServerId(const RoomId id) const
    {
        return getRawRoomRef(id).get()->server_id;
    }

public:
    void setPosition(const RoomId id, const Coordinate &coord)
    {
        getRawRoomRef(id).getMutable()->position = coord;
    }
    NODISCARD const Coordinate &getPosition(const RoomId id) const
    {
        return getRawRoomRef(id).get()->position;
    }

public:
    NODISCARD RoomStatusEnum getStatus(const RoomId id) const
    {
        return getRawRoomRef(id).get()->status;
    }
    void setStatus(const RoomId id, RoomStatusEnum status)
    {
        auto raw_room_ptr = getRawRoomRef(id).getMutable();
        if (status != raw_room_ptr->status) {
            raw_room_ptr->status = status;
        }
    }

public:
    NODISCARD bool operator==(const RawRooms &rhs) const
    {
        // Rely on IndexedVector::operator== which uses areEquivalent.
        // areEquivalent correctly handles different sizes by checking
        // if extra elements in the longer vector are default-constructed T{}.
        // This requires mm::CopyOnWrite<RawRoom>::operator== to be defined, which it now is.
        // A default-constructed mm::CopyOnWrite<RawRoom> holds a default-constructed RawRoom.
        // RawRooms::removeAt() sets the element to mm::CopyOnWrite<RawRoom>(std::make_shared<RawRoom>()),
        // which is also what a default mm::CopyOnWrite<RawRoom> results in.
        return m_rooms == rhs.m_rooms;
    }
    NODISCARD bool operator!=(const RawRooms &rhs) const
    {
        return !(*this == rhs);
    }
};
