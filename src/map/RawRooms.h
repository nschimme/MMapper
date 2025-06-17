#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2024 The MMapper Authors

#include "../global/IndexedVector.h"
#include "../global/macros.h"
#include "CowRoom.h" // Added
#include "InvalidMapOperation.h"
#include "RawExit.h"
#include "RawRoom.h"

#include <memory> // Added

class NODISCARD RawRooms final
{
private:
    IndexedVector<CowRoom, RoomId> m_rooms; // Changed RawRoom to CowRoom

public:
    NODISCARD CowRoom &getRawRoomRef(RoomId pos) { return m_rooms.at(pos); } // Return CowRoom&
    NODISCARD const CowRoom &getRawRoomRef(RoomId pos) const
    {
        return m_rooms.at(pos);
    } // Return const CowRoom&

public:
    NODISCARD size_t size() const { return m_rooms.size(); }
    void resize(const size_t numRooms) { m_rooms.resize(numRooms); }

    void removeAt(const RoomId id)
    {
        getRawRoomRef(id) = CowRoom(std::make_shared<RawRoom>());
    } // Updated removeAt

    void requireUninitialized(const RoomId id) const
    {
        if (*getRawRoomRef(id).get() != RawRoom{}) { // Updated requireUninitialized
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
        if (m_rooms.size() != rhs.m_rooms.size()) {
            return false;
        }
        for (RoomId i = RoomId{0}; i < RoomId{static_cast<RoomId::WrappedType>(m_rooms.size())};
             ++i) {
            // Assuming RoomId can be cast to size_t or int for loop
            // Also assuming m_rooms.isValid(i) or similar check might be needed if vector can be sparse
            // For now, direct iteration up to size.
            const auto &r1 = getRawRoomRef(i);
            const auto &r2 = rhs.getRawRoomRef(i);
            if (!r1.get() || !r2.get()) { // one of them is uninitialized via CowRoom
                if (r1.get() != r2.get())
                    return false; // only true if both are null
            } else if (*r1.get() != *r2.get()) {
                return false;
            }
        }
        return true;
    }
    NODISCARD bool operator!=(const RawRooms &rhs) const
    {
        return !(*this == rhs);
    }
};
