// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#ifdef _WIN32
#  include <windows.h>
#endif
#undef TRANSPARENT
#undef HORIZONTAL
#undef VERTICAL
#undef constant

#include "RoomDataBuffer.h"

#include <glm/gtc/type_ptr.hpp>

#include "../global/progresscounter.h"
#include "../global/utils.h"
#include "../map/Map.h"
#include "../map/RawRoom.h"
#include "../map/World.h"
#include "../map/enums.h"
#include "../map/exit.h"
#include "../map/room.h"
#include "../opengl/legacy/Legacy.h"
#include "../opengl/legacy/Meshes.h"
#include "../opengl/legacy/Shaders.h"
#include "../opengl/legacy/SimpleMesh.h"
#include "RoadIndex.h"

enum class WallOrientationEnum { OrientationHorizontal, OrientationVertical };

static NamedColorEnum getWallNamedColorCommon(const ExitFlags flags,
                                              const WallOrientationEnum wallOrientation)
{
    const bool isVertical = wallOrientation == WallOrientationEnum::OrientationVertical;
    if (isVertical && flags.isClimb()) {
        return NamedColorEnum::VERTICAL_COLOR_CLIMB;
    }
    if (flags.isNoFlee())
        return NamedColorEnum::WALL_COLOR_NO_FLEE;
    if (flags.isRandom())
        return NamedColorEnum::WALL_COLOR_RANDOM;
    if (flags.isFall() || flags.isDamage())
        return NamedColorEnum::WALL_COLOR_FALL_DAMAGE;
    if (flags.isSpecial())
        return NamedColorEnum::WALL_COLOR_SPECIAL;
    if (flags.isClimb())
        return NamedColorEnum::WALL_COLOR_CLIMB;
    if (flags.isGuarded())
        return NamedColorEnum::WALL_COLOR_GUARDED;
    if (flags.isNoMatch())
        return NamedColorEnum::WALL_COLOR_NO_MATCH;
    return NamedColorEnum::TRANSPARENT;
}

RoomDataBuffer::RoomDataBuffer(const Legacy::SharedFunctions &sharedFuncs)
    : m_sharedFuncs(sharedFuncs)
{
    auto &prog = deref(m_sharedFuncs).getShaderPrograms().getMegaRoomShader();
    m_mesh = std::make_unique<Legacy::MegaRoomMesh<MegaRoomVert>>(m_sharedFuncs, prog);
}

RoomDataBuffer::~RoomDataBuffer() = default;

void RoomDataBuffer::resize(size_t newSize)
{
    if (newSize <= m_capacity)
        return;
    m_capacity = newSize;
    m_cpuBuffer.resize(m_capacity);
    m_mesh->setStatic(DrawModeEnum::INSTANCED_QUADS, m_cpuBuffer);
}

MegaRoomVert RoomDataBuffer::packRoom(const RoomHandle &room,
                                      const mctp::MapCanvasTexturesProxy &textures)
{
    MegaRoomVert v{};
    if (!room.exists())
        return v;

    v.pos = room.getPosition().to_ivec3();
    v.highlight = static_cast<uint32_t>(NamedColorEnum::TRANSPARENT);

    // Terrain and Trail
    const auto roomTerrainType = room.getTerrainType();
    const RoadIndexMaskEnum roadIndex = getRoadIndex(room.getRaw());

    int terrainIdx = 0;
    if (roomTerrainType == RoomTerrainEnum::ROAD) {
        terrainIdx = textures.road[roadIndex].position;
    } else {
        terrainIdx = textures.terrain[roomTerrainType].position;
    }

    int trailIdx = 0xFFFF;
    if (roadIndex != RoadIndexMaskEnum::NONE && roomTerrainType != RoomTerrainEnum::ROAD) {
        trailIdx = textures.trail[roadIndex].position;
    }

    v.terrain_trail = (static_cast<uint32_t>(trailIdx) << 16)
                      | (static_cast<uint32_t>(terrainIdx) & 0xFFFF);

    // Flags
    uint32_t f = 8u; // active bit
    if (room.getLightType() == RoomLightEnum::DARK)
        f |= 1u;
    if (room.getSundeathType() == RoomSundeathEnum::NO_SUNDEATH)
        f |= 2u;
    if (room.getRidableType() == RoomRidableEnum::NOT_RIDABLE)
        f |= 4u;
    v.flags = f;

    // Overlays
    v.mob_flags = room.getMobFlags().asUint32();
    v.load_flags = room.getLoadFlags().asUint32();

    // Walls
    for (uint32_t i = 0; i < 6u; ++i) {
        const ExitDirEnum dir = ALL_EXITS_NESWUD[static_cast<size_t>(i)];
        const auto &exit = room.getExit(dir);
        const ExitFlags flags = exit.getExitFlags();

        uint32_t type = 0; // None
        NamedColorEnum color = NamedColorEnum::TRANSPARENT;
        bool isFlow = flags.isFlow();
        bool isClimb = flags.isClimb();

        if (flags.isExit()) {
            if (flags.isDoor()) {
                type = 3; // DOOR
                color = NamedColorEnum::WALL_COLOR_REGULAR_EXIT;
            } else {
                const auto wallColor =
                    (i < 4) ? getWallNamedColorCommon(flags, WallOrientationEnum::OrientationHorizontal)
                            : getWallNamedColorCommon(flags, WallOrientationEnum::OrientationVertical);
                if (wallColor != NamedColorEnum::TRANSPARENT) {
                    type = 2; // DOTTED
                    color = wallColor;
                } else {
                    type = 1; // SOLID
                    color = (i < 4) ? NamedColorEnum::WALL_COLOR_REGULAR_EXIT
                                    : NamedColorEnum::VERTICAL_COLOR_REGULAR_EXIT;
                }
            }
        } else if (!exit.outIsEmpty()) {
            type = 2; // DOTTED
            color = NamedColorEnum::WALL_COLOR_BUG_WALL_DOOR;
        }

        bool isExit = flags.isExit();
        uint32_t info = (type & 0xF) | (static_cast<uint32_t>(color) << 4) | (isFlow ? 0x1000u : 0u)
                        | (isClimb ? 0x2000u : 0u) | (isExit ? 0x4000u : 0u);
        v.wall_info[i / 2] |= (info << (16 * (i % 2)));
    }

    return v;
}

void RoomDataBuffer::setHighlights(const std::unordered_map<RoomId, NamedColorEnum> &highlights)
{
    if (m_capacity == 0)
        return;

    std::vector<uint32_t> changedIndices;
    for (const auto &[id, color] : highlights) {
        const uint32_t idx = id.asUint32();
        if (idx < m_capacity) {
            const uint32_t colorVal = static_cast<uint32_t>(color);
            if (m_cpuBuffer[idx].highlight != colorVal) {
                m_cpuBuffer[idx].highlight = colorVal;
                changedIndices.push_back(idx);
            }
        }
    }

    if (changedIndices.empty())
        return;

    // Optimization: if many changed, just update all?
    if (changedIndices.size() > m_capacity / 2) {
        m_mesh->update(0, m_cpuBuffer);
    } else {
        // Individual updates
        for (const uint32_t idx : changedIndices) {
            std::vector<MegaRoomVert> batch = {m_cpuBuffer[idx]};
            m_mesh->update(static_cast<qopengl_GLintptr>(idx), batch);
        }
    }
}

void RoomDataBuffer::syncWithMap(const Map &map, const mctp::MapCanvasTexturesProxy &textures)
{
    const size_t nextId = map.getWorld().getNextId().asUint32();
    if (nextId > m_capacity) {
        resize(nextId);
        m_initialized = false;
    }

    if (!m_initialized) {
        m_cpuBuffer.assign(m_capacity, MegaRoomVert{});
        map.getRooms().for_each([&](const RoomId id) {
            m_cpuBuffer[id.asUint32()] = packRoom(map.getRoomHandle(id), textures);
        });
        m_mesh->update(0, m_cpuBuffer);
        m_initialized = true;
    } else {
        std::vector<RoomId> changed;
        ProgressCounter dummy;
        Map::foreachChangedRoom(dummy, m_lastMap, map, [&](const RawRoom &room) {
            changed.push_back(room.getId());
        });

        for (const auto id : changed) {
            const uint32_t idx = id.asUint32();
            MegaRoomVert v = packRoom(map.getRoomHandle(id), textures);
            // preserve highlight
            v.highlight = m_cpuBuffer[idx].highlight;
            m_cpuBuffer[idx] = v;
            std::vector<MegaRoomVert> batch = {v};
            m_mesh->update(static_cast<qopengl_GLintptr>(idx), batch);
        }

        // Deletions
        m_lastMap.getRooms().for_each([&](const RoomId id) {
            if (!map.findRoomHandle(id)) {
                const uint32_t idx = id.asUint32();
                m_cpuBuffer[idx] = MegaRoomVert{};
                std::vector<MegaRoomVert> batch = {m_cpuBuffer[idx]};
                m_mesh->update(static_cast<qopengl_GLintptr>(idx), batch);
            }
        });
    }
    m_lastMap = map;
}

void RoomDataBuffer::render(OpenGL &gl,
                            const glm::mat4 &mvp,
                            int currentLayer,
                            bool drawUpperLayersTextured,
                            const Color &timeOfDayColor)
{
    if (m_capacity == 0)
        return;
    std::ignore = mvp;

    auto &prog = deref(m_sharedFuncs).getShaderPrograms().getMegaRoomShader();

    prog->currentLayer = currentLayer;
    prog->drawUpperLayersTextured = drawUpperLayersTextured;

    m_mesh->render(gl.getDefaultRenderState()
                       .withBlend(BlendModeEnum::TRANSPARENCY)
                       .withTimeOfDayColor(timeOfDayColor));
}
