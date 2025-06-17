#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2023 The MMapper Authors

#include "RawRoom.h" // Assuming RawRoom.h is in the same directory
#include <memory>
#include <variant>

// Forward declaration if RawRoom is in a namespace
// namespace mm { class RawRoom; }

class CowRoom {
private:
    std::variant<std::shared_ptr<const RawRoom>, std::shared_ptr<RawRoom>> m_room_variant;

public:
    // Default constructor: Initializes with a default, writable RawRoom
    CowRoom() : m_room_variant(std::make_shared<RawRoom>()) {}

    // Constructor: Initializes with a read-only RawRoom
    CowRoom(std::shared_ptr<const RawRoom> room) : m_room_variant(room) {}

    // Constructor: Initializes with a writable RawRoom (e.g., a new room)
    CowRoom(std::shared_ptr<RawRoom> room) : m_room_variant(room) {}

    // Gets a read-only pointer to the RawRoom.
    std::shared_ptr<const RawRoom> get() const {
        if (std::holds_alternative<std::shared_ptr<const RawRoom>>(m_room_variant)) {
            return std::get<std::shared_ptr<const RawRoom>>(m_room_variant);
        } else {
            // If it's writable, it can be safely converted to const
            return std::get<std::shared_ptr<RawRoom>>(m_room_variant);
        }
    }

    // Gets a writable pointer to the RawRoom.
    // Performs a lazy copy if the current instance is read-only (holds std::shared_ptr<const RawRoom>).
    // If the instance already holds a std::shared_ptr<RawRoom> (i.e., nominally writable),
    // a deep copy is still made if that specific shared_ptr instance is shared (use_count > 1).
    // This ensures true copy-on-write semantics and prevents unintended side-effects
    // on other CowRoom instances that might happen to share this data block, even if it's
    // marked as writable by one of them. The goal is that a call to getMutable()
    // should generally provide a uniquely owned RawRoom to the caller if modifications are expected.
    std::shared_ptr<RawRoom> getMutable() {
        if (std::holds_alternative<std::shared_ptr<const RawRoom>>(m_room_variant)) {
            // Lazy copy: create a new RawRoom by copying the old one
            auto const_ptr = std::get<std::shared_ptr<const RawRoom>>(m_room_variant);
            auto writable_ptr = std::make_shared<RawRoom>(*const_ptr); // Assumes RawRoom has a copy constructor
            m_room_variant = writable_ptr; // Update this instance to point to the new copy
            return writable_ptr;
        } else { // Already holds std::shared_ptr<RawRoom>
            std::shared_ptr<RawRoom>& current_writable_ptr = std::get<std::shared_ptr<RawRoom>>(m_room_variant);
            if (current_writable_ptr.use_count() > 1) {
                // If shared, perform a copy
                auto new_writable_ptr = std::make_shared<RawRoom>(*current_writable_ptr);
                m_room_variant = new_writable_ptr; // Update this instance to point to the new copy
                return new_writable_ptr;
            }
            // If not shared (use_count == 1), return the existing writable pointer
            return current_writable_ptr;
        }
    }

    // Finalizes mutations by converting the writable instance back to a read-only instance.
    // This is typically called after modifications are done.
    void finalize() {
        if (std::holds_alternative<std::shared_ptr<RawRoom>>(m_room_variant)) {
            // Convert writable to read-only
            m_room_variant = std::static_pointer_cast<const RawRoom>(std::get<std::shared_ptr<RawRoom>>(m_room_variant));
        }
        // If it's already const, do nothing
    }

    // Optional: A way to check if the current instance is writable
    bool isWritable() const {
        return std::holds_alternative<std::shared_ptr<RawRoom>>(m_room_variant);
    }
};
