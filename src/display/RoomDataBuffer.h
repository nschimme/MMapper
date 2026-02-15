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
#include <unordered_set>
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
    std::vector<MegaRoomVert> m_cpuBuffer;
    std::unordered_set<RoomId> m_activeHighlights;
    size_t m_capacity = 0;
    Map m_lastMap;
    bool m_initialized = false;
    bool m_lastDrawUnmappedExits = false;
    GLRenderState m_lastRenderState;

public:
    explicit RoomDataBuffer(const Legacy::SharedFunctions &sharedFuncs);
    ~RoomDataBuffer();

    void syncWithMap(const Map &map, const mctp::MapCanvasTexturesProxy &textures, bool drawUnmappedExits);
    void setHighlights(const std::unordered_map<RoomId, NamedColorEnum> &highlights);
    void beginRender(OpenGL &gl,
                     const glm::mat4 &mvp,
                     const Color &timeOfDayColor,
                     const mctp::MapCanvasTexturesProxy &textures);
    void renderLayers(int minZ,
                      int maxZ,
                      int currentLayer,
                      bool drawUpperLayersTextured,
                      const glm::vec2 &minBounds,
                      const glm::vec2 &maxBounds);

private:
    void resize(size_t newSize);
    static MegaRoomVert packRoom(const RoomHandle &room,
                                 const mctp::MapCanvasTexturesProxy &textures);
};
