// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "GLDoors.h"

#include "legacy/Legacy.h"

#include <algorithm>

GLDoors::GLDoors(GameObserver &observer, Legacy::UboManager &uboManager)
    : m_observer{observer}
    , m_uboManager{uboManager}
{
    m_lifetime = m_observer.sig2_doorStateChanged.connect(
        [this](ServerRoomId id, ExitDirEnum dir, DoorStateEnum state) {
            onDoorStateChanged(id, dir, state);
        });

    m_uboManager.registerRebuildFunction(Legacy::SharedVboEnum::DoorBlock,
                                         [this](Legacy::Functions &gl) { rebuildUbo(gl); });
}

GLDoors::~GLDoors()
{
    m_uboManager.unregisterRebuildFunction(Legacy::SharedVboEnum::DoorBlock);
}

void GLDoors::onDoorStateChanged(ServerRoomId id, ExitDirEnum dir, DoorStateEnum state)
{
    const DoorKey key{id, dir};

    // Find if already in deque
    auto it = std::find_if(m_doors.begin(), m_doors.end(), [&key](const DoorEntry &e) {
        return e.key == key;
    });

    if (state == DoorStateEnum::CLOSED) {
        if (it != m_doors.end()) {
            m_doors.erase(it);
            m_uboManager.invalidate(Legacy::SharedVboEnum::DoorBlock);
        }
    } else {
        if (it != m_doors.end()) {
            if (it->state != state) {
                it->state = state;
                m_uboManager.invalidate(Legacy::SharedVboEnum::DoorBlock);
            }
            // Move to newest? User agreed cascaded updates are bad, but UBO rebuild is cheap for 100 entries.
            // Let's stick to simple FIFO/update-in-place for now as agreed.
        } else {
            if (m_doors.size() >= 100) {
                m_doors.pop_front();
            }
            m_doors.push_back({key, state});
            m_uboManager.invalidate(Legacy::SharedVboEnum::DoorBlock);
        }
    }
}

void GLDoors::rebuildUbo(Legacy::Functions &gl)
{
    Legacy::DoorBlock block{};
    for (size_t i = 0; i < m_doors.size(); ++i) {
        const auto &entry = m_doors[i];
        block.doors[i] = glm::uvec4(entry.key.roomId.asUint32(),
                                    static_cast<uint32_t>(entry.key.dir),
                                    static_cast<uint32_t>(entry.state),
                                    0);
    }
    // Rest of block.doors is zero-initialized (roomId=0 is invalid).
    m_uboManager.update<Legacy::SharedVboEnum::DoorBlock>(gl, block);
}
