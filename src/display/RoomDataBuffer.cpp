// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "RoomDataBuffer.h"

#include "../global/progresscounter.h"
#include "../global/utils.h"
#include "../map/Map.h"
#include "../map/RawRoom.h"
#include "../map/World.h"
#include "../map/enums.h"
#include "../map/exit.h"
#include "../map/room.h"
#include "../opengl/legacy/Legacy.h"
#include "../opengl/legacy/Shaders.h"
#include "../opengl/legacy/SimpleMesh.h"
#include "RoadIndex.h"

#include <glm/gtc/type_ptr.hpp>

#define VOIDPTR_OFFSETOF(x, y) reinterpret_cast<void *>(offsetof(x, y))
#define VPO(x) VOIDPTR_OFFSETOF(VertexType_, x)

namespace Legacy {

template<typename VertexType_>
class NODISCARD MegaRoomMesh final : public SimpleMesh<VertexType_, MegaRoomShader>
{
public:
    using Base = SimpleMesh<VertexType_, MegaRoomShader>;
    using Base::Base;

private:
    struct NODISCARD Attribs final
    {
        GLuint pos = INVALID_ATTRIB_LOCATION;
        GLuint terrain_trail = INVALID_ATTRIB_LOCATION;
        GLuint flags = INVALID_ATTRIB_LOCATION;
        GLuint mob_flags = INVALID_ATTRIB_LOCATION;
        GLuint load_flags = INVALID_ATTRIB_LOCATION;
        GLuint wall_info0 = INVALID_ATTRIB_LOCATION;
        GLuint wall_info1 = INVALID_ATTRIB_LOCATION;
        GLuint wall_info2 = INVALID_ATTRIB_LOCATION;

        NODISCARD static Attribs getLocations(MegaRoomShader &shader)
        {
            Attribs r;
            r.pos = shader.getAttribLocation("aPos");
            r.terrain_trail = shader.getAttribLocation("aTerrainTrail");
            r.flags = shader.getAttribLocation("aFlags");
            r.mob_flags = shader.getAttribLocation("aMobFlags");
            r.load_flags = shader.getAttribLocation("aLoadFlags");
            r.wall_info0 = shader.getAttribLocation("aWallInfo0");
            r.wall_info1 = shader.getAttribLocation("aWallInfo1");
            r.wall_info2 = shader.getAttribLocation("aWallInfo2");
            return r;
        }
    };

    std::optional<Attribs> m_boundAttribs;

    void virt_bind() override
    {
        const auto vertSize = static_cast<GLsizei>(sizeof(VertexType_));
        Functions &gl = Base::m_functions;
        const auto attribs = Attribs::getLocations(Base::m_program);
        gl.glBindBuffer(GL_ARRAY_BUFFER, Base::m_vbo.get());

        gl.enableAttribI(attribs.pos, 3, GL_INT, vertSize, VPO(pos));
        gl.enableAttribI(attribs.terrain_trail, 1, GL_UNSIGNED_INT, vertSize, VPO(terrain_trail));
        gl.enableAttribI(attribs.flags, 1, GL_UNSIGNED_INT, vertSize, VPO(flags));
        gl.enableAttribI(attribs.mob_flags, 1, GL_UNSIGNED_INT, vertSize, VPO(mob_flags));
        gl.enableAttribI(attribs.load_flags, 1, GL_UNSIGNED_INT, vertSize, VPO(load_flags));
        gl.enableAttribI(attribs.wall_info0, 1, GL_UNSIGNED_INT, vertSize, VPO(wall_info[0]));
        gl.enableAttribI(attribs.wall_info1, 1, GL_UNSIGNED_INT, vertSize, VPO(wall_info[1]));
        gl.enableAttribI(attribs.wall_info2, 1, GL_UNSIGNED_INT, vertSize, VPO(wall_info[2]));

        // instancing
        gl.glVertexAttribDivisor(attribs.pos, 1);
        gl.glVertexAttribDivisor(attribs.terrain_trail, 1);
        gl.glVertexAttribDivisor(attribs.flags, 1);
        gl.glVertexAttribDivisor(attribs.mob_flags, 1);
        gl.glVertexAttribDivisor(attribs.load_flags, 1);
        gl.glVertexAttribDivisor(attribs.wall_info0, 1);
        gl.glVertexAttribDivisor(attribs.wall_info1, 1);
        gl.glVertexAttribDivisor(attribs.wall_info2, 1);

        m_boundAttribs = attribs;
    }

    void virt_unbind() override
    {
        if (!m_boundAttribs) {
            assert(false);
            return;
        }

        auto &attribs = m_boundAttribs.value();
        Functions &gl = Base::m_functions;
        gl.glDisableVertexAttribArray(attribs.pos);
        gl.glDisableVertexAttribArray(attribs.terrain_trail);
        gl.glDisableVertexAttribArray(attribs.flags);
        gl.glDisableVertexAttribArray(attribs.mob_flags);
        gl.glDisableVertexAttribArray(attribs.load_flags);
        gl.glDisableVertexAttribArray(attribs.wall_info0);
        gl.glDisableVertexAttribArray(attribs.wall_info1);
        gl.glDisableVertexAttribArray(attribs.wall_info2);
        gl.glBindBuffer(GL_ARRAY_BUFFER, 0);
        m_boundAttribs.reset();
    }
};

} // namespace Legacy

#undef VOIDPTR_OFFSETOF
#undef VPO

enum class WallOrientationEnum { HORIZONTAL, VERTICAL };

static NamedColorEnum getWallNamedColorCommon(const ExitFlags flags,
                                              const WallOrientationEnum wallOrientation)
{
    const bool isVertical = wallOrientation == WallOrientationEnum::VERTICAL;
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
    std::vector<MegaRoomVert> empty(m_capacity);
    m_mesh->setStatic(DrawModeEnum::INSTANCED_QUADS, empty);
}

MegaRoomVert RoomDataBuffer::packRoom(const RoomHandle &room,
                                      const mctp::MapCanvasTexturesProxy &textures)
{
    MegaRoomVert v{};
    if (!room.exists())
        return v;

    v.pos = room.getPosition().to_ivec3();

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
    for (int i = 0; i < 6; ++i) {
        const ExitDirEnum dir = ALL_EXITS_NESWUD[i];
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
                const auto wallColor = (i < 4)
                                           ? getWallNamedColorCommon(flags,
                                                                     WallOrientationEnum::HORIZONTAL)
                                           : getWallNamedColorCommon(flags,
                                                                     WallOrientationEnum::VERTICAL);
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

        uint32_t info = (type & 0xF) | (static_cast<uint32_t>(color) << 4) | (isFlow ? 0x1000u : 0u)
                        | (isClimb ? 0x2000u : 0u);
        v.wall_info[i / 2] |= (info << (16 * (i % 2)));
    }

    return v;
}

void RoomDataBuffer::syncWithMap(const Map &map, const mctp::MapCanvasTexturesProxy &textures)
{
    const size_t nextId = map.getWorld().getNextId().asUint32();
    if (nextId > m_capacity) {
        resize(nextId);
        m_initialized = false;
    }

    if (!m_initialized) {
        std::vector<MegaRoomVert> all(m_capacity);
        map.getRooms().for_each([&](const RoomId id) {
            all[id.asUint32()] = packRoom(map.getRoomHandle(id), textures);
        });
        m_mesh->update(0, all);
        m_initialized = true;
    } else {
        std::vector<RoomId> changed;
        ProgressCounter dummy;
        Map::foreachChangedRoom(dummy, m_lastMap, map, [&](const RawRoom &room) {
            changed.push_back(room.getId());
        });

        for (const auto id : changed) {
            std::vector<MegaRoomVert> batch = {packRoom(map.getRoomHandle(id), textures)};
            m_mesh->update(id.asUint32(), batch);
        }

        // Deletions
        m_lastMap.getRooms().for_each([&](const RoomId id) {
            if (!map.findRoomHandle(id)) {
                std::vector<MegaRoomVert> batch = {MegaRoomVert{}}; // clear it
                m_mesh->update(id.asUint32(), batch);
            }
        });
    }
    m_lastMap = map;
}

void RoomDataBuffer::render(OpenGL &gl,
                            const glm::mat4 & /*mvp*/,
                            int currentLayer,
                            bool drawUpperLayersTextured)
{
    if (m_capacity == 0)
        return;

    auto &prog = deref(m_sharedFuncs).getShaderPrograms().getMegaRoomShader();

    auto unbinder = prog->bind();

    // Set extra uniforms
    prog->setInt("uCurrentLayer", currentLayer);
    prog->setBool("uDrawUpperLayersTextured", drawUpperLayersTextured);

    m_mesh->render(gl.getDefaultRenderState().withBlend(BlendModeEnum::TRANSPARENCY));
}
