#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2024 The MMapper Authors

#include "../global/macros.h"
#include <immer/vector.hpp>
#include <immer/vector_transient.hpp> // For efficient bulk updates if needed
#include "InvalidMapOperation.h"
#include "RawExit.h"
#include "RawRoom.h"

class NODISCARD RawRooms final
{
private:
    immer::vector<RawRoom> m_rooms;

    // Helper to convert RoomId to size_t, assuming RoomId can be an index
    // This might need adjustment based on RoomId's actual range and nature.
    // If RoomIds are sparse, immer::vector might not be the best fit directly
    // and a map (immer::map<RoomId, RawRoom>) might be better.
    // For now, proceeding with direct index assumption from IndexedVector.
    static size_t to_idx(RoomId id) { return id.asUint32(); }

public:
    // getRawRoomRef returns a const ref. For modification, need to use 'set'.
    NODISCARD const RawRoom &getRawRoomRef(RoomId pos) const
    {
        // immer::vector::operator[] provides const reference access.
        // Add bounds checking if necessary, similar to std::vector::at().
        if (to_idx(pos) >= m_rooms.size()) {
            throw InvalidMapOperation(); // Or std::out_of_range
        }
        return m_rooms[to_idx(pos)];
    }

    // Modification of a room now requires updating the persistent vector:
    // e.g., m_rooms = m_rooms.set(to_idx(id), new_room_value);
    // So, direct non-const ref is not possible with immer's COW semantics.
    // Methods wanting to modify a room will need to take RoomId and new value,
    // or operate on a copy and then use 'set'.

public:
    NODISCARD size_t size() const { return m_rooms.size(); }

    void resize(const size_t numRooms)
    {
        // immer::vector doesn't have a direct resize like std::vector.
        // If numRooms > current size, append default RawRooms.
        // If numRooms < current size, take a slice.
        if (numRooms > m_rooms.size()) {
            auto transient = m_rooms.transient();
            for(size_t i = m_rooms.size(); i < numRooms; ++i) {
                transient.push_back(RawRoom{});
            }
            m_rooms = transient.persistent();
        } else if (numRooms < m_rooms.size()) {
            m_rooms = m_rooms.take(numRooms);
        }
    }

    void removeAt(const RoomId id)
    {
        // Replaces the room at id with a default-constructed RawRoom.
        if (to_idx(id) < m_rooms.size()) {
            m_rooms = m_rooms.set(to_idx(id), RawRoom{});
        } else {
            // Or throw, or resize if id is just beyond current size.
            // Consistent with original getRawRoomRef(id) = RawRoom{};
            // which would throw if id is out of bounds for m_rooms.at().
            throw InvalidMapOperation();
        }
    }

    void requireUninitialized(const RoomId id) const
    {
        if (to_idx(id) < m_rooms.size() && getRawRoomRef(id) != RawRoom{}) {
            throw InvalidMapOperation();
        }
        // If id >= m_rooms.size(), it's considered uninitialized.
    }

    // Helper for methods that need to modify a room.
    void setRawRoom(RoomId id, const RawRoom& room_input)
    {
        RawRoom room_copy = room_input; // Make a mutable copy
        // Ensure invariants are applied to the copy before it's stored.
        // This calls the RawRooms::enforceInvariantsOnCopy(RawRoom&, RoomId) method.
        enforceInvariantsOnCopy(room_copy, id);

        if (to_idx(id) >= m_rooms.size()) {
            // This case needs careful handling. If IndexedVector allowed sparse-like
            // access that automatically resized, this logic needs to replicate it.
            // For now, assume id must be within current valid range or just at the end.
            if (to_idx(id) == m_rooms.size()) {
                 m_rooms = m_rooms.push_back(room_copy); // Store the modified copy
            } else {
                 throw InvalidMapOperation(); // Or resize appropriately
            }
        } else {
            m_rooms = m_rooms.set(to_idx(id), room_copy); // Store the modified copy
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
        if (to_idx(id) < m_rooms.size()) { \
            if (getRawRoomRef(id).fields._Name != x) { \
                RawRoom room = getRawRoomRef(id); \
                room.fields._Name = std::move(x); \
                m_rooms = m_rooms.set(to_idx(id), room); \
            } \
        } else { throw InvalidMapOperation(); /* Or handle resize */ } \
    }

    XFOREACH_ROOM_STRING_PROPERTY(X_DECL_ACCESSORS)
    XFOREACH_ROOM_FLAG_PROPERTY(X_DECL_ACCESSORS)
    XFOREACH_ROOM_ENUM_PROPERTY(X_DECL_ACCESSORS)
#undef X_DECL_ACCESSORS

public:
#define X_DEFINE_ACCESSOR(_Type, _Name, _Init) \
    void setExit##_Type(const RoomId id, const ExitDirEnum dir, _Type x) \
    { \
        if (to_idx(id) < m_rooms.size()) { \
            RawRoom room = getRawRoomRef(id); \
            if (room.getExit(dir).fields._Name != x) { \
                room.getExit(dir).fields._Name = std::move(x); \
                m_rooms = m_rooms.set(to_idx(id), room); \
            } \
        } else { throw InvalidMapOperation(); /* Or handle resize */ } \
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
        if (to_idx(id) < m_rooms.size()) {
            RawRoom room = getRawRoomRef(id);
            room.getExit(dir).outgoing = set;
            // enforceInvariants might modify the room further.
            // It needs to be pure or also return a new RawRoom.
            // For now, assume enforceInvariants is tricky with COW and may need refactoring.
            // This is a deep issue with COW integration.
            // Let's assume for now that enforceInvariants can be called on a mutable copy
            // before it's set back.
            enforceInvariantsOnCopy(room, id, dir); // Hypothetical refactor
            m_rooms = m_rooms.set(to_idx(id), room);
        } else { throw InvalidMapOperation(); }
    }
    NODISCARD const TinyRoomIdSet &getExitOutgoing(const RoomId id, const ExitDirEnum dir) const
    {
        return getRawRoomRef(id).getExit(dir).outgoing;
    }

public:
    void setExitIncoming(const RoomId id, const ExitDirEnum dir, const TinyRoomIdSet &set)
    {
        if (to_idx(id) < m_rooms.size()) {
            RawRoom room = getRawRoomRef(id);
            room.getExit(dir).incoming = set;
            m_rooms = m_rooms.set(to_idx(id), room);
        } else { throw InvalidMapOperation(); }
    }
    NODISCARD const TinyRoomIdSet &getExitIncoming(const RoomId id, const ExitDirEnum dir) const
    {
        return getRawRoomRef(id).getExit(dir).incoming;
    }

public:
    // This function's signature implies modification. It will need to be adapted.
    // void setExitFlags_safe(RoomId id, ExitDirEnum dir, ExitFlags input_flags);
    // For now, assume it's handled in .cpp or refactored.

public:
    // enforceInvariants would ideally be const or return a new RawRoom.
    // If it modifies RawRoom in-place, it's problematic for COW.
    // These will require careful refactoring in the .cpp.
    // void enforceInvariants(RoomId id, ExitDirEnum dir);
    // void enforceInvariants(RoomId id);
    // Hypothetical refactored version for setExitOutgoing:
    void enforceInvariantsOnCopy(RawRoom& mutable_room_copy, RoomId id, ExitDirEnum dir) const;
    void enforceInvariantsOnCopy(RawRoom& mutable_room_copy, RoomId id) const;

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
        if (to_idx(id) < m_rooms.size()) {
            RawRoom room = getRawRoomRef(id);
            const bool isOut = inOut == InOutEnum::Out;
            auto &data = room.getExit(dir); // Modifying copy
            (isOut ? data.outgoing : data.incoming) = set;
            if (isOut) {
                // Again, enforceInvariants needs careful handling.
                enforceInvariantsOnCopy(room, id, dir); // On copy
            }
            m_rooms = m_rooms.set(to_idx(id), room);
        } else { throw InvalidMapOperation(); }
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
        if (to_idx(id) < m_rooms.size()) {
            RawRoom room = getRawRoomRef(id);
            if (room.server_id != serverId) {
                 room.server_id = serverId;
                 m_rooms = m_rooms.set(to_idx(id), room);
            }
        } else { throw InvalidMapOperation(); }
    }
    NODISCARD const ServerRoomId &getServerId(const RoomId id) const
    {
        return getRawRoomRef(id).server_id;
    }

public:
    void setPosition(const RoomId id, const Coordinate &coord)
    {
        if (to_idx(id) < m_rooms.size()) {
            RawRoom room = getRawRoomRef(id);
            if (room.position != coord) {
                room.position = coord;
                m_rooms = m_rooms.set(to_idx(id), room);
            }
        } else { throw InvalidMapOperation(); }
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
        if (to_idx(id) < m_rooms.size()) {
            if (getRawRoomRef(id).status != status) {
                RawRoom room = getRawRoomRef(id);
                room.status = status;
                m_rooms = m_rooms.set(to_idx(id), room);
            }
        } else { throw InvalidMapOperation(); }
    }

public:
    NODISCARD bool operator==(const RawRooms &rhs) const
    {
        return m_rooms == rhs.m_rooms;
    }
    NODISCARD bool operator!=(const RawRooms &rhs) const
    {
        return !(rhs == *this);
    }
};
