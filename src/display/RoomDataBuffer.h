// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#pragma once

#include "../map/Map.h"
#include "../map/roomid.h"
#include "../opengl/OpenGL.h"
#include "../opengl/OpenGLTypes.h"
#include "../opengl/legacy/Legacy.h"
#include "Textures.h"

#include <memory>
#include <vector>

namespace Legacy {
template<typename T>
class MegaRoomMesh;
}

class RoomDataBuffer final
{
private:
    Legacy::SharedFunctions m_sharedFuncs;
    std::unique_ptr<Legacy::MegaRoomMesh<MegaRoomVert>> m_mesh;
    size_t m_capacity = 0;
    Map m_lastMap;
    bool m_initialized = false;

public:
    explicit RoomDataBuffer(const Legacy::SharedFunctions &sharedFuncs);
    ~RoomDataBuffer();

    void syncWithMap(const Map &map, const mctp::MapCanvasTexturesProxy &textures);
    void render(OpenGL &gl, const glm::mat4 &mvp, int currentLayer, bool drawUpperLayersTextured);

private:
    void resize(size_t newSize);
    static MegaRoomVert packRoom(const RoomHandle &room,
                                 const mctp::MapCanvasTexturesProxy &textures);
};
