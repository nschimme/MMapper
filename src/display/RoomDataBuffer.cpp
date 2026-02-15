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

#include "../configuration/configuration.h"
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
#include "mapcanvas.h"

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

    const bool drawUnmappedExits = getConfig().canvas.showUnmappedExits.get();

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
    v.flags = f;

    // Overlays (up to 8)
    uint32_t o1 = 0xFFFFFFFFu;
    uint32_t o2 = 0xFFFFFFFFu;
    uint32_t oCount = 0;
    auto addOverlay = [&](int idx) {
        if (oCount < 4) {
            uint32_t shift = 8 * oCount;
            o1 = (o1 & ~(0xFFu << shift)) | (static_cast<uint32_t>(idx) << shift);
        } else if (oCount < 8) {
            uint32_t shift = 8 * (oCount - 4);
            o2 = (o2 & ~(0xFFu << shift)) | (static_cast<uint32_t>(idx) << shift);
        }
        oCount++;
    };

    room.getLoadFlags().for_each([&](const RoomLoadFlagEnum flag) {
        addOverlay(textures.load[flag].position);
    });
    room.getMobFlags().for_each([&](const RoomMobFlagEnum flag) {
        addOverlay(textures.mob[flag].position);
    });
    if (room.getRidableType() == RoomRidableEnum::NOT_RIDABLE) {
        addOverlay(textures.no_ride.position);
    }
    v.overlays1 = o1;
    v.overlays2 = o2;

    // Walls
    const Map &map = room.getMap();
    for (uint32_t i = 0; i < 6u; ++i) {
        const ExitDirEnum dir = ALL_EXITS_NESWUD[static_cast<size_t>(i)];
        const auto &exit = room.getExit(dir);
        const ExitFlags flags = exit.getExitFlags();

        uint32_t type = 0; // None
        NamedColorEnum color = NamedColorEnum::TRANSPARENT;
        bool isOutFlow = flags.isFlow();
        bool isClimb = flags.isClimb();

        if (drawUnmappedExits && exit.exitIsUnmapped()) {
            type = 2; // DOTTED
            color = NamedColorEnum::WALL_COLOR_NOT_MAPPED;
        } else if (flags.isExit()) {
            if (flags.isDoor()) {
                type = 3; // DOOR
                color = NamedColorEnum::WALL_COLOR_REGULAR_EXIT;
            } else {
                const auto wallColor = (i < 4) ? getWallNamedColorCommon(flags, WallOrientationEnum::OrientationHorizontal)
                                               : getWallNamedColorCommon(flags, WallOrientationEnum::OrientationVertical);
                if (wallColor != NamedColorEnum::TRANSPARENT) {
                    type = 2; // DOTTED
                    color = wallColor;
                }
            }
        } else {
            // Not an exit -> Wall
            if (!exit.outIsEmpty()) {
                type = 2; // DOTTED BUG WALL
                color = NamedColorEnum::WALL_COLOR_BUG_WALL_DOOR;
            } else {
                type = 1; // SOLID
                color = (i < 4) ? NamedColorEnum::WALL_COLOR_REGULAR_EXIT
                                : NamedColorEnum::VERTICAL_COLOR_REGULAR_EXIT;
            }
        }

        // InFlow detection
        bool isInFlow = false;
        if (!exit.inIsEmpty()) {
            for (const RoomId targetRoomId : exit.getIncomingSet()) {
                const auto &targetRoom = map.getRoomHandle(targetRoomId);
                for (const ExitDirEnum targetDir : ALL_EXITS_NESWUD) {
                    const auto &targetExit = targetRoom.getExit(targetDir);
                    if (targetExit.getExitFlags().isFlow() && targetExit.containsOut(room.getId())) {
                        isInFlow = true;
                        break;
                    }
                }
                if (isInFlow)
                    break;
            }
        }

        bool isExit = flags.isExit();
        uint32_t info = (type & 0x7) | (static_cast<uint32_t>(color) << 3) | (isOutFlow ? 0x800u : 0u)
                        | (isClimb ? 0x1000u : 0u) | (isExit ? 0x2000u : 0u) | (isInFlow ? 0x4000u : 0u);
        // info: bits 0-2 type, 3-10 color, 11 outFlow, 12 climb, 13 exit, 14 inFlow
        // Fits in 15 bits.
        v.wall_info[i / 2] |= (info << (16 * (i % 2)));
    }

    return v;
}

void RoomDataBuffer::setHighlights(const std::unordered_map<RoomId, NamedColorEnum> &highlights)
{
    if (m_capacity == 0)
        return;

    std::vector<uint32_t> changedIndices;

    // Clear old highlights that are not in the new set
    for (auto it = m_activeHighlights.begin(); it != m_activeHighlights.end();) {
        RoomId id = *it;
        if (highlights.find(id) == highlights.end()) {
            uint32_t idx = id.asUint32();
            if (idx < m_capacity) {
                m_cpuBuffer[idx].highlight = static_cast<uint32_t>(NamedColorEnum::TRANSPARENT);
                changedIndices.push_back(idx);
            }
            it = m_activeHighlights.erase(it);
        } else {
            ++it;
        }
    }

    // Add/Update new highlights
    for (const auto &[id, color] : highlights) {
        const uint32_t idx = id.asUint32();
        if (idx < m_capacity) {
            const uint32_t colorVal = static_cast<uint32_t>(color);
            if (m_cpuBuffer[idx].highlight != colorVal) {
                m_cpuBuffer[idx].highlight = colorVal;
                changedIndices.push_back(idx);
                m_activeHighlights.insert(id);
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

void RoomDataBuffer::syncWithMap(const Map &map,
                                 const mctp::MapCanvasTexturesProxy &textures,
                                 bool drawUnmappedExits)
{
    const size_t nextId = map.getWorld().getNextId().asUint32();
    if (nextId > m_capacity) {
        resize(nextId);
        m_initialized = false;
    }

    // If drawUnmappedExits changed, we must repack everything
    static bool lastDrawUnmapped = false;
    if (drawUnmappedExits != lastDrawUnmapped) {
        m_initialized = false;
        lastDrawUnmapped = drawUnmappedExits;
    }

    if (!m_initialized) {
        m_cpuBuffer.assign(m_capacity, MegaRoomVert{});
        m_activeHighlights.clear();
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
                m_activeHighlights.erase(id);
                std::vector<MegaRoomVert> batch = {m_cpuBuffer[idx]};
                m_mesh->update(static_cast<qopengl_GLintptr>(idx), batch);
            }
        });
    }
    m_lastMap = map;
}

void RoomDataBuffer::renderLayer(OpenGL &gl,
                                 const glm::mat4 & /*mvp*/,
                                 int z,
                                 int currentLayer,
                                 bool drawUpperLayersTextured,
                                 const Color &timeOfDayColor,
                                 const glm::vec2 &minBounds,
                                 const glm::vec2 &maxBounds)
{
    if (m_capacity == 0)
        return;

    auto &prog = deref(m_sharedFuncs).getShaderPrograms().getMegaRoomShader();
    const auto &textures = mctp::getProxy(deref(MapCanvas::getPrimary()).getTextures());

    prog->currentLayer = currentLayer;
    prog->drawUpperLayersTextured = drawUpperLayersTextured;
    prog->minBounds = minBounds;
    prog->maxBounds = maxBounds;
    prog->drawLayer = z;

    // Texture IDs
    prog->uTerrainTex = textures.terrain[RoomTerrainEnum::UNDEFINED].array;
    prog->uTrailTex = textures.trail[RoadIndexMaskEnum::NONE].array;
    prog->uOverlayTex = textures.mob[RoomMobFlagEnum::RENT].array;
    prog->uWallTex = textures.wall[ExitDirEnum::NORTH].array;
    prog->uDottedWallTex = textures.dotted_wall[ExitDirEnum::NORTH].array;
    prog->uDoorTex = textures.door[ExitDirEnum::NORTH].array;
    prog->uStreamInTex = textures.stream_in[ExitDirEnum::NORTH].array;
    prog->uStreamOutTex = textures.stream_out[ExitDirEnum::NORTH].array;
    prog->uExitTex = textures.exit_up.array;
    prog->uWhiteTex = textures.white_pixel.array;

    // Layer indices
    for (size_t i = 0; i < 4; ++i) {
        prog->uWallLayers[i] = textures.wall[ALL_EXITS_NESWUD[i]].position;
        prog->uDottedWallLayers[i] = textures.dotted_wall[ALL_EXITS_NESWUD[i]].position;
    }
    for (size_t i = 0; i < 6; ++i) {
        prog->uDoorLayers[i] = textures.door[ALL_EXITS_NESWUD[i]].position;
        prog->uStreamInLayers[i] = textures.stream_in[ALL_EXITS_NESWUD[i]].position;
        prog->uStreamOutLayers[i] = textures.stream_out[ALL_EXITS_NESWUD[i]].position;
    }
    prog->uExitLayers[0] = textures.exit_climb_down.position;
    prog->uExitLayers[1] = textures.exit_climb_up.position;
    prog->uExitLayers[2] = textures.exit_down.position;
    prog->uExitLayers[3] = textures.exit_up.position;

    m_mesh->render(gl.getDefaultRenderState()
                       .withBlend(BlendModeEnum::TRANSPARENCY)
                       .withTimeOfDayColor(timeOfDayColor));
}
