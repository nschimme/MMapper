// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Nils Schimmelmann <nschimme@gmail.com> (Jahara)

#include "MapCanvasRoomDrawer.h"

#include "../configuration/NamedConfig.h"
#include "../configuration/configuration.h"
#include "../global/Array.h"
#include "../global/ConfigConsts.h"
#include "../global/EnumIndexedArray.h"
#include "../global/Flags.h"
#include "../global/RuleOf5.h"
#include "../global/progresscounter.h"
#include "../global/utils.h"
#include "../map/DoorFlags.h"
#include "../map/ExitFieldVariant.h"
#include "../map/ExitFlags.h"
#include "../map/Map.h"
#include "../map/coordinate.h"
#include "../map/enums.h"
#include "../map/exit.h"
#include "../map/infomark.h"
#include "../map/mmapper2room.h"
#include "../map/room.h"
#include "../mapdata/mapdata.h"
#include "../opengl/FontFormatFlags.h"
#include "../opengl/OpenGL.h"
#include "../opengl/OpenGLTypes.h"
#include "../opengl/modern/RoomInstanceData.h"
#include "ConnectionLineBuilder.h"
#include "MapCanvasData.h"
#include "RoadIndex.h"
#include "mapcanvas.h" // hack, since we're now definining some of its symbols

#include <cassert>
#include <cstdlib>
#include <memory>
#include <optional>
#include <set>
#include <stdexcept>
#include <unordered_map>
#include <vector>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <QColor>
#include <QMessageLogContext>
#include <QtGui/qopengl.h>
#include <QtGui>

struct NODISCARD VisitRoomOptions final
{
    SharedCanvasNamedColorOptions canvasColors;
    SharedNamedColorOptions colorSettings;
    bool drawNotMappedExits = false;
};

enum class NODISCARD StreamTypeEnum { OutFlow, InFlow };
enum class NODISCARD WallTypeEnum { SOLID, DOTTED, DOOR };
static constexpr const size_t NUM_WALL_TYPES = 3;
DEFINE_ENUM_COUNT(WallTypeEnum, NUM_WALL_TYPES)

#define LOOKUP_COLOR(X) (getNamedColorOptions().X)

// must be called from the main thread
NODISCARD static VisitRoomOptions getVisitRoomOptions()
{
    const auto &config = getConfig();
    auto &canvas = config.canvas;
    VisitRoomOptions result;
    result.canvasColors = canvas.clone();
    result.colorSettings = config.colorSettings.clone();
    result.drawNotMappedExits = canvas.showUnmappedExits.get();
    return result;
}

NODISCARD static bool isTransparent(const XNamedColor &namedColor)
{
    return !namedColor.isInitialized() || namedColor == LOOKUP_COLOR(TRANSPARENT);
}

NODISCARD static std::optional<Color> getColor(const XNamedColor &namedColor)
{
    if (isTransparent(namedColor)) {
        return std::nullopt;
    }
    return namedColor.getColor();
}

enum class NODISCARD WallOrientationEnum { HORIZONTAL, VERTICAL };
NODISCARD static XNamedColor getWallNamedColorCommon(const ExitFlags flags,
                                                     const WallOrientationEnum wallOrientation)
{
    const bool isVertical = wallOrientation == WallOrientationEnum::VERTICAL;

    // Vertical colors override the horizontal case
    // REVISIT: consider using the same set and just override the color
    // using the same order of flag testing as the horizontal case.
    //
    // In other words, eliminate this `if (isVertical)` block and just use
    //   return (isVertical ? LOOKUP_COLOR(VERTICAL_COLOR_CLIMB) : LOOKUP_COLOR(WALL_COLOR_CLIMB));
    // in the appropriate places in the following chained test for flags.
    if (isVertical) {
        if (flags.isClimb()) {
            // NOTE: This color is slightly darker than WALL_COLOR_CLIMB
            return LOOKUP_COLOR(VERTICAL_COLOR_CLIMB);
        }
        /*FALL-THRU*/
    }

    if (flags.isNoFlee()) {
        return LOOKUP_COLOR(WALL_COLOR_NO_FLEE);
    } else if (flags.isRandom()) {
        return LOOKUP_COLOR(WALL_COLOR_RANDOM);
    } else if (flags.isFall() || flags.isDamage()) {
        return LOOKUP_COLOR(WALL_COLOR_FALL_DAMAGE);
    } else if (flags.isSpecial()) {
        return LOOKUP_COLOR(WALL_COLOR_SPECIAL);
    } else if (flags.isClimb()) {
        return LOOKUP_COLOR(WALL_COLOR_CLIMB);
    } else if (flags.isGuarded()) {
        return LOOKUP_COLOR(WALL_COLOR_GUARDED);
    } else if (flags.isNoMatch()) {
        return LOOKUP_COLOR(WALL_COLOR_NO_MATCH);
    } else {
        return LOOKUP_COLOR(TRANSPARENT);
    }
}

NODISCARD static XNamedColor getWallNamedColor(const ExitFlags flags)
{
    return getWallNamedColorCommon(flags, WallOrientationEnum::HORIZONTAL);
}

NODISCARD static XNamedColor getVerticalNamedColor(const ExitFlags flags)
{
    return getWallNamedColorCommon(flags, WallOrientationEnum::VERTICAL);
}

struct LayerBatchData;

struct NODISCARD TerrainAndTrail final
{
    MMTexArrayPosition terrain;
    MMTexArrayPosition trail;
};

NODISCARD static TerrainAndTrail getRoomTerrainAndTrail(const mctp::MapCanvasTexturesProxy &textures,
                                                        const RawRoom &room)
{
    const auto roomTerrainType = room.getTerrainType();
    const RoadIndexMaskEnum roadIndex = getRoadIndex(room);

    TerrainAndTrail result;
    result.terrain = (roomTerrainType == RoomTerrainEnum::ROAD) ? textures.road[roadIndex]
                                                                : textures.terrain[roomTerrainType];

    if (roadIndex != RoadIndexMaskEnum::NONE && roomTerrainType != RoomTerrainEnum::ROAD) {
        result.trail = textures.trail[roadIndex];
    }
    return result;
}

struct NODISCARD IRoomVisitorCallbacks
{
public:
    virtual ~IRoomVisitorCallbacks();

private:
    NODISCARD virtual bool virt_acceptRoom(const RoomHandle &) const = 0;

private:
    // Rooms
    virtual void virt_visitTerrainTexture(const RoomHandle &, const MMTexArrayPosition &) = 0;
    virtual void virt_visitTrailTexture(const RoomHandle &, const MMTexArrayPosition &) = 0;
    virtual void virt_visitOverlayTexture(const RoomHandle &, const MMTexArrayPosition &) = 0;
    virtual void virt_visitNamedColorTint(const RoomHandle &, RoomTintEnum) = 0;

    // Walls
    virtual void virt_visitWall(
        const RoomHandle &, ExitDirEnum, const XNamedColor &, WallTypeEnum, bool isClimb)
        = 0;

    // Streams
    virtual void virt_visitStream(const RoomHandle &, ExitDirEnum, StreamTypeEnum) = 0;

public:
    NODISCARD bool acceptRoom(const RoomHandle &room) const { return virt_acceptRoom(room); }

    // Rooms
    void visitTerrainTexture(const RoomHandle &room, const MMTexArrayPosition &tex)
    {
        virt_visitTerrainTexture(room, tex);
    }
    void visitTrailTexture(const RoomHandle &room, const MMTexArrayPosition &tex)
    {
        virt_visitTrailTexture(room, tex);
    }
    void visitOverlayTexture(const RoomHandle &room, const MMTexArrayPosition &tex)
    {
        virt_visitOverlayTexture(room, tex);
    }
    void visitNamedColorTint(const RoomHandle &room, const RoomTintEnum tint)
    {
        virt_visitNamedColorTint(room, tint);
    }

    // Walls
    void visitWall(const RoomHandle &room,
                   const ExitDirEnum dir,
                   const XNamedColor &color,
                   const WallTypeEnum wallType,
                   const bool isClimb)
    {
        virt_visitWall(room, dir, color, wallType, isClimb);
    }

    // Streams
    void visitStream(const RoomHandle &room, const ExitDirEnum dir, StreamTypeEnum streamType)
    {
        virt_visitStream(room, dir, streamType);
    }
};

IRoomVisitorCallbacks::~IRoomVisitorCallbacks() = default;

static void visitRoom(const RoomHandle &room,
                      const mctp::MapCanvasTexturesProxy &textures,
                      IRoomVisitorCallbacks &callbacks,
                      const VisitRoomOptions &visitRoomOptions)
{
    if (!callbacks.acceptRoom(room)) {
        return;
    }

    // const auto &pos = room.getPosition();
    const bool isDark = room.getLightType() == RoomLightEnum::DARK;
    const bool hasNoSundeath = room.getSundeathType() == RoomSundeathEnum::NO_SUNDEATH;
    const bool notRideable = room.getRidableType() == RoomRidableEnum::NOT_RIDABLE;
    const auto terrainAndTrail = getRoomTerrainAndTrail(textures, room.getRaw());
    const RoomMobFlags mf = room.getMobFlags();
    const RoomLoadFlags lf = room.getLoadFlags();

    callbacks.visitTerrainTexture(room, terrainAndTrail.terrain);

    if (auto trail = terrainAndTrail.trail; trail.array != INVALID_MM_TEXTURE_ID) {
        callbacks.visitTrailTexture(room, trail);
    }

    if (isDark) {
        callbacks.visitNamedColorTint(room, RoomTintEnum::DARK);
    } else if (hasNoSundeath) {
        callbacks.visitNamedColorTint(room, RoomTintEnum::NO_SUNDEATH);
    }

    mf.for_each([&room, &textures, &callbacks](const RoomMobFlagEnum flag) -> void {
        callbacks.visitOverlayTexture(room, textures.mob[flag]);
    });

    lf.for_each([&room, &textures, &callbacks](const RoomLoadFlagEnum flag) -> void {
        callbacks.visitOverlayTexture(room, textures.load[flag]);
    });

    if (notRideable) {
        callbacks.visitOverlayTexture(room, textures.no_ride);
    }

    const Map &map = room.getMap();
    const auto drawInFlow = [&map, &room, &callbacks](const RawExit &exit,
                                                      const ExitDirEnum &dir) -> void {
        // For each incoming connections
        for (const RoomId targetRoomId : exit.getIncomingSet()) {
            const auto &targetRoom = map.getRoomHandle(targetRoomId);
            for (const ExitDirEnum targetDir : ALL_EXITS_NESWUD) {
                const auto &targetExit = targetRoom.getExit(targetDir);
                const ExitFlags flags = targetExit.getExitFlags();
                if (flags.isFlow() && targetExit.containsOut(room.getId())) {
                    callbacks.visitStream(room, dir, StreamTypeEnum::InFlow);
                    return;
                }
            }
        }
    };

    // drawExit()

    // FIXME: This requires a map update.
    // REVISIT: The logic of drawNotMappedExits seems a bit wonky.
    for (const ExitDirEnum dir : ALL_EXITS_NESW) {
        const auto &exit = room.getExit(dir);
        const ExitFlags flags = exit.getExitFlags();
        const auto isExit = flags.isExit();
        const auto isDoor = flags.isDoor();

        const bool isClimb = flags.isClimb();

        // FIXME: This requires a map update.
        // TODO: make "not mapped" exits a separate mesh;
        // except what should we do for the "else" case?
        if (visitRoomOptions.drawNotMappedExits && exit.exitIsUnmapped()) {
            callbacks.visitWall(room,
                                dir,
                                LOOKUP_COLOR(WALL_COLOR_NOT_MAPPED),
                                WallTypeEnum::DOTTED,
                                isClimb);
        } else {
            const auto namedColor = getWallNamedColor(flags);
            if (!isTransparent(namedColor)) {
                callbacks.visitWall(room, dir, namedColor, WallTypeEnum::DOTTED, isClimb);
            }

            if (flags.isFlow()) {
                callbacks.visitStream(room, dir, StreamTypeEnum::OutFlow);
            }
        }

        // wall
        if (!isExit || isDoor) {
            if (!isDoor && !exit.outIsEmpty()) {
                callbacks.visitWall(room,
                                    dir,
                                    LOOKUP_COLOR(WALL_COLOR_BUG_WALL_DOOR),
                                    WallTypeEnum::DOTTED,
                                    isClimb);
            } else {
                callbacks.visitWall(room,
                                    dir,
                                    LOOKUP_COLOR(WALL_COLOR_REGULAR_EXIT),
                                    WallTypeEnum::SOLID,
                                    isClimb);
            }
        }
        // door
        if (isDoor) {
            callbacks.visitWall(room,
                                dir,
                                LOOKUP_COLOR(WALL_COLOR_REGULAR_EXIT),
                                WallTypeEnum::DOOR,
                                isClimb);
        }

        if (!exit.inIsEmpty()) {
            drawInFlow(exit, dir);
        }
    }

    // drawVertical
    for (const ExitDirEnum dir : {ExitDirEnum::UP, ExitDirEnum::DOWN}) {
        const auto &exit = room.getExit(dir);
        const auto &flags = exit.getExitFlags();
        const bool isClimb = flags.isClimb();

        if (visitRoomOptions.drawNotMappedExits && flags.isUnmapped()) {
            callbacks.visitWall(room,
                                dir,
                                LOOKUP_COLOR(WALL_COLOR_NOT_MAPPED),
                                WallTypeEnum::DOTTED,
                                isClimb);
            continue;
        } else if (!flags.isExit()) {
            continue;
        }

        // NOTE: in the "old" version, this falls-thru and the custom color is overwritten
        // by the regular exit; so using if-else here is a bug fix.
        const auto namedColor = getVerticalNamedColor(flags);
        if (!isTransparent(namedColor)) {
            callbacks.visitWall(room, dir, namedColor, WallTypeEnum::DOTTED, isClimb);
        } else {
            callbacks.visitWall(room,
                                dir,
                                LOOKUP_COLOR(VERTICAL_COLOR_REGULAR_EXIT),
                                WallTypeEnum::SOLID,
                                isClimb);
        }

        if (flags.isDoor()) {
            callbacks.visitWall(room,
                                dir,
                                LOOKUP_COLOR(WALL_COLOR_REGULAR_EXIT),
                                WallTypeEnum::DOOR,
                                isClimb);
        }

        if (flags.isFlow()) {
            callbacks.visitStream(room, dir, StreamTypeEnum::OutFlow);
        }

        if (!exit.inIsEmpty()) {
            drawInFlow(exit, dir);
        }
    }
}

static void visitRooms(const RoomVector &rooms,
                       const mctp::MapCanvasTexturesProxy &textures,
                       IRoomVisitorCallbacks &callbacks,
                       const VisitRoomOptions &visitRoomOptions)
{
    DECL_TIMER(t, __FUNCTION__);
    for (const auto &room : rooms) {
        visitRoom(room, textures, callbacks, visitRoomOptions);
    }
}

struct NODISCARD RoomTex
{
    RoomHandle room;
    MMTexArrayPosition pos;

    explicit RoomTex(RoomHandle moved_room, const MMTexArrayPosition &input_pos)
        : room{std::move(moved_room)}
        , pos{input_pos}
    {
        if (input_pos.array == INVALID_MM_TEXTURE_ID)
            throw std::invalid_argument("input_pos");
    }

    NODISCARD static bool isPartitioned(const RoomTex &a, const RoomTex &b)
    {
        return a.pos.array < b.pos.array;
    }
};

struct NODISCARD ColoredRoomTex : public RoomTex
{
    Color color;
    ColoredRoomTex(const RoomHandle &room, const MMTexArrayPosition &tex) = delete;

    explicit ColoredRoomTex(RoomHandle moved_room,
                            const MMTexArrayPosition input_pos,
                            const Color &input_color)
        : RoomTex{std::move(moved_room), input_pos}
        , color{input_color}
    {}
};

// Caution: Although O(n) partitioning into an array indexed by constant number of texture IDs
// is theoretically faster than O(n log n) sorting, one naive attempt to prematurely optimize
// this code resulted in a 50x slow-down.
//
// Note: sortByTexture() probably won't ever be a performance bottleneck for the default map,
// since at the time of this comment, the full O(n log n) vector sort only takes up about 2%
// of the total runtime of the mesh generation.
//
// Conclusion: Look elsewhere for optimization opportunities -- at least until profiling says
// that sorting is at significant fraction of the total runtime.
using RoomTexVector = std::vector<RoomTex>;
using ColoredRoomTexVector = std::vector<ColoredRoomTex>;

using InstanceMap = std::unordered_map<MMTextureId, std::vector<modern::RoomInstanceData>>;

static void createInstanceMeshes(LayerMeshesIntermediate::FnVec &meshes,
                                 const std::string_view what,
                                 const InstanceMap &instances)
{
    if (instances.empty()) {
        return;
    }

    if constexpr (IS_DEBUG_BUILD) {
        if (instances.size() > 1) {
            MMLOG() << what << " has " << instances.size() << " unique textures\n";
        }
    }

    meshes.reserve(meshes.size() + instances.size());

    for (auto const &[textureId, instanceVector] : instances) {
        auto v_ptr = std::make_shared<std::vector<modern::RoomInstanceData>>(instanceVector);
        meshes.emplace_back([v = v_ptr, t = textureId](OpenGL &g) {
            return g.createInstancedRoomBatch(*v, t);
        });
    }
}

struct NODISCARD LayerBatchData final
{
    InstanceMap batch_less_blended_transparent;
    InstanceMap batch_lequal_blended_transparent;
    InstanceMap batch_equal_blended_transparent;
    RoomTintArray<PlainQuadBatch> roomTints;
    PlainQuadBatch roomLayerBoostQuads;

    // So it can be used in std containers
    explicit LayerBatchData() = default;

    ~LayerBatchData() = default;
    DEFAULT_MOVES_DELETE_COPIES(LayerBatchData);

    NODISCARD LayerMeshesIntermediate buildIntermediate() const
    {
        DECL_TIMER(t2, "LayerBatchData::buildIntermediate");
        LayerMeshesIntermediate meshes;
        createInstanceMeshes(meshes.batch_less_blended_transparent,
                             "batch_less_blended_transparent",
                             batch_less_blended_transparent);
        createInstanceMeshes(meshes.batch_lequal_blended_transparent,
                             "batch_lequal_blended_transparent",
                             batch_lequal_blended_transparent);
        createInstanceMeshes(meshes.batch_equal_blended_transparent,
                             "batch_equal_blended_transparent",
                             batch_equal_blended_transparent);
        meshes.tints = roomTints; // REVISIT: this is a copy instead of a move
        meshes.layerBoost = roomLayerBoostQuads; // REVISIT: this is a copy instead of a move
        meshes.isValid = true;
        return meshes;
    }

    NODISCARD LayerMeshes getMeshes(OpenGL &gl) const
    {
        DECL_TIMER(t, "LayerBatchData::getMeshes");
        return buildIntermediate().getLayerMeshes(gl);
    }
};

class NODISCARD LayerBatchBuilder final : public IRoomVisitorCallbacks
{
private:
    LayerBatchData &m_data;
    const mctp::MapCanvasTexturesProxy &m_textures;
    const OptBounds &m_bounds;

public:
    explicit LayerBatchBuilder(LayerBatchData &data,
                               const mctp::MapCanvasTexturesProxy &textures,
                               const OptBounds &bounds)
        : m_data{data}
        , m_textures{textures}
        , m_bounds{bounds}
    {}

    ~LayerBatchBuilder() final;

    DELETE_CTORS_AND_ASSIGN_OPS(LayerBatchBuilder);

private:
    NODISCARD bool virt_acceptRoom(const RoomHandle &room) const final
    {
        return m_bounds.contains(room.getPosition());
    }

    void virt_visitTerrainTexture(const RoomHandle &room, const MMTexArrayPosition &terrain) final
    {
        if (terrain.array == INVALID_MM_TEXTURE_ID) {
            return;
        }

        modern::RoomInstanceData instance;
        instance.position = room.getPosition().to_vec3();
        instance.tex_coord = glm::vec3(0.0f, 0.0f, static_cast<float>(terrain.position));
        instance.color = glm::vec4(1.0f);
        m_data.batch_less_blended_transparent[terrain.array].push_back(instance);


        const auto v0 = room.getPosition().to_vec3();
#define EMIT(x, y) m_data.roomLayerBoostQuads.emplace_back(v0 + glm::vec3((x), (y), 0))
        EMIT(0, 0);
        EMIT(1, 0);
        EMIT(1, 1);
        EMIT(0, 1);
#undef EMIT
    }

    void virt_visitTrailTexture(const RoomHandle &room, const MMTexArrayPosition &trail) final
    {
        if (trail.array != INVALID_MM_TEXTURE_ID) {
            modern::RoomInstanceData instance;
            instance.position = room.getPosition().to_vec3();
            instance.tex_coord = glm::vec3(0.0f, 0.0f, static_cast<float>(trail.position));
            instance.color = glm::vec4(1.0f);
            m_data.batch_equal_blended_transparent[trail.array].push_back(instance);
        }
    }

    void virt_visitOverlayTexture(const RoomHandle &room, const MMTexArrayPosition &overlay) final
    {
        if (overlay.array != INVALID_MM_TEXTURE_ID) {
            modern::RoomInstanceData instance;
            instance.position = room.getPosition().to_vec3();
            instance.tex_coord = glm::vec3(0.0f, 0.0f, static_cast<float>(overlay.position));
            instance.color = glm::vec4(1.0f);
            m_data.batch_equal_blended_transparent[overlay.array].push_back(instance);
        }
    }

    void virt_visitNamedColorTint(const RoomHandle &room, const RoomTintEnum tint) final
    {
        const auto v0 = room.getPosition().to_vec3();
#define EMIT(x, y) m_data.roomTints[tint].emplace_back(v0 + glm::vec3((x), (y), 0))
        EMIT(0, 0);
        EMIT(1, 0);
        EMIT(1, 1);
        EMIT(0, 1);
#undef EMIT
    }

    void virt_visitWall(const RoomHandle &room,
                        const ExitDirEnum dir,
                        const XNamedColor &color,
                        const WallTypeEnum wallType,
                        const bool isClimb) final
    {
        if (isTransparent(color)) {
            return;
        }

        const std::optional<Color> optColor = getColor(color);
        if (!optColor.has_value()) {
            assert(false);
            return;
        }

        const auto glcolor = optColor.value().getVec4();

        modern::RoomInstanceData instance;
        instance.position = room.getPosition().to_vec3();
        instance.color = glcolor;

        if (wallType == WallTypeEnum::DOOR) {
            const MMTexArrayPosition &tex = m_textures.door[dir];
            instance.tex_coord = glm::vec3(0.0f, 0.0f, static_cast<float>(tex.position));
            m_data.batch_lequal_blended_transparent[tex.array].push_back(instance);

        } else {
            if (isNESW(dir)) {
                if (wallType == WallTypeEnum::SOLID) {
                    const MMTexArrayPosition &tex = m_textures.wall[dir];
                    instance.tex_coord = glm::vec3(0.0f, 0.0f, static_cast<float>(tex.position));
                    m_data.batch_lequal_blended_transparent[tex.array].push_back(instance);
                } else {
                    const MMTexArrayPosition &tex = m_textures.dotted_wall[dir];
                    instance.tex_coord = glm::vec3(0.0f, 0.0f, static_cast<float>(tex.position));
                    m_data.batch_lequal_blended_transparent[tex.array].push_back(instance);
                }
            } else {
                const bool isUp = dir == ExitDirEnum::UP;
                assert(isUp || dir == ExitDirEnum::DOWN);

                const MMTexArrayPosition &tex = isClimb ? (isUp ? m_textures.exit_climb_up
                                                                : m_textures.exit_climb_down)
                                                        : (isUp ? m_textures.exit_up
                                                                : m_textures.exit_down);

                instance.tex_coord = glm::vec3(0.0f, 0.0f, static_cast<float>(tex.position));
                m_data.batch_equal_blended_transparent[tex.array].push_back(instance);
            }
        }
    }

    void virt_visitStream(const RoomHandle &room,
                          const ExitDirEnum dir,
                          const StreamTypeEnum type) final
    {
        const auto color = LOOKUP_COLOR(STREAM).getColor().getVec4();
        modern::RoomInstanceData instance;
        instance.position = room.getPosition().to_vec3();
        instance.color = color;

        switch (type) {
        case StreamTypeEnum::OutFlow: {
            const MMTexArrayPosition &tex = m_textures.stream_out[dir];
            instance.tex_coord = glm::vec3(0.0f, 0.0f, static_cast<float>(tex.position));
            m_data.batch_lequal_blended_transparent[tex.array].push_back(instance);
            return;
        }
        case StreamTypeEnum::InFlow: {
            const MMTexArrayPosition &tex = m_textures.stream_in[dir];
            instance.tex_coord = glm::vec3(0.0f, 0.0f, static_cast<float>(tex.position));
            m_data.batch_lequal_blended_transparent[tex.array].push_back(instance);
            return;
        }
        default:
            break;
        }

        assert(false);
    }
};

LayerBatchBuilder::~LayerBatchBuilder() = default;

NODISCARD static LayerMeshesIntermediate generateLayerMeshes(
    const RoomVector &rooms,
    const mctp::MapCanvasTexturesProxy &textures,
    const OptBounds &bounds,
    const VisitRoomOptions &visitRoomOptions)
{
    DECL_TIMER(t, "generateLayerMeshes");

    LayerBatchData data;
    LayerBatchBuilder builder{data, textures, bounds};
    visitRooms(rooms, textures, builder, visitRoomOptions);

    return data.buildIntermediate();
}

struct NODISCARD InternalData final : public IMapBatchesFinisher
{
public:
    std::unordered_map<int, LayerMeshesIntermediate> batchedMeshes;
    BatchedConnections connectionDrawerBuffers;
    std::unordered_map<int, RoomNameBatchIntermediate> roomNameBatches;

private:
    void virt_finish(MapBatches &output, OpenGL &gl, GLFont &font) const final;
};

static void generateAllLayerMeshes(InternalData &internalData,
                                   const FontMetrics &font,
                                   const LayerToRooms &layerToRooms,
                                   const mctp::MapCanvasTexturesProxy &textures,
                                   const VisitRoomOptions &visitRoomOptions)

{
    // This feature has been removed, but it's passed to a lot of functions,
    // so it would be annoying to have to add it back if we decide the feature was
    // actually necessary.
    const OptBounds bounds{};

    DECL_TIMER(t, "generateAllLayerMeshes");
    auto &batchedMeshes = internalData.batchedMeshes;
    auto &connectionDrawerBuffers = internalData.connectionDrawerBuffers;
    auto &roomNameBatches = internalData.roomNameBatches;

    for (const auto &layer : layerToRooms) {
        DECL_TIMER(t2, "generateAllLayerMeshes.loop");
        const int thisLayer = layer.first;
        auto &layerMeshes = batchedMeshes[thisLayer];
        ConnectionDrawerBuffers &cdb = connectionDrawerBuffers[thisLayer];
        RoomNameBatch rnb;
        const auto &rooms = layer.second;

        {
            DECL_TIMER(t3, "generateAllLayerMeshes.loop.part2");
            layerMeshes = ::generateLayerMeshes(rooms, textures, bounds, visitRoomOptions);
        }

        {
            DECL_TIMER(t4, "generateAllLayerMeshes.loop.part3");

            // TODO: move everything in the same layer to the same internal struct?
            cdb.clear();
            rnb.clear();

            ConnectionDrawer cd{cdb, rnb, thisLayer, bounds};
            {
                DECL_TIMER(t7, "generateAllLayerMeshes.loop.part3b");
                // pass 2: add to buffers
                for (const auto &room : rooms) {
                    cd.drawRoomConnectionsAndDoors(room);
                }
            }
        }

        {
            DECL_TIMER(t8, "generateAllLayerMeshes.loop.part4");
            roomNameBatches[thisLayer] = rnb.getIntermediate(font);
        }
    }
}

LayerMeshes LayerMeshesIntermediate::getLayerMeshes(OpenGL &gl) const
{
    if (!isValid) {
        return {};
    }

    struct NODISCARD Resolver final
    {
        OpenGL &m_gl;
        NODISCARD UniqueMeshVector resolve(const FnVec &v)
        {
            std::vector<UniqueMesh> result;
            result.reserve(v.size());
            for (const auto &f : v) {
                result.emplace_back(f(m_gl));
            }
            return UniqueMeshVector{std::move(result)};
        }
        NODISCARD UniqueMesh resolve(const PlainQuadBatch &batch)
        {
            return m_gl.createPlainQuadBatch(batch);
        }
        NODISCARD RoomTintArray<UniqueMesh> resolve(const RoomTintArray<PlainQuadBatch> &arr)
        {
            RoomTintArray<UniqueMesh> result;
            for (const RoomTintEnum tint : ALL_ROOM_TINTS) {
                const PlainQuadBatch &b = arr[tint];
                result[tint] = m_gl.createPlainQuadBatch(b);
            }
            return result;
        }
    };

    DECL_TIMER(t, "LayerMeshesIntermediate::getLayerMeshes");
    Resolver r{gl};
    LayerMeshes lm;
    lm.batch_less_blended_transparent = r.resolve(batch_less_blended_transparent);
    lm.batch_lequal_blended_transparent = r.resolve(batch_lequal_blended_transparent);
    lm.batch_equal_blended_transparent = r.resolve(batch_equal_blended_transparent);
    lm.tints = r.resolve(tints);
    lm.layerBoost = r.resolve(layerBoost);
    lm.isValid = true;

    return lm;
}

void LayerMeshes::render(const int thisLayer, const int focusedLayer)
{
    bool disableTextures = false;
    if (thisLayer > focusedLayer) {
        if (!getConfig().canvas.drawUpperLayersTextured) {
            disableTextures = true;
        }
    }

    const GLRenderState less_blended =
        GLRenderState().withDepthFunction(DepthFunctionEnum::LESS).withBlend(
            BlendModeEnum::TRANSPARENCY);
    const GLRenderState lequal_blended =
        GLRenderState().withDepthFunction(DepthFunctionEnum::LEQUAL).withBlend(
            BlendModeEnum::TRANSPARENCY);
    const GLRenderState equal_blended =
        GLRenderState().withDepthFunction(DepthFunctionEnum::EQUAL).withBlend(
            BlendModeEnum::TRANSPARENCY);
    const GLRenderState equal_multiplied =
        GLRenderState().withDepthFunction(DepthFunctionEnum::EQUAL).withBlend(
            BlendModeEnum::MODULATE);

    const auto color = std::invoke([&thisLayer, &focusedLayer]() -> Color {
        if (thisLayer <= focusedLayer) {
            return Colors::white.withAlpha(0.90f);
        }
        return Colors::gray70.withAlpha(0.20f);
    });

    if (disableTextures) {
        const auto layerWhite = Colors::white.withAlpha(0.20f);
        layerBoost.render(less_blended.withColor(layerWhite));
    } else {
        batch_less_blended_transparent.render(less_blended.withColor(color));
    }

    for (const RoomTintEnum tint : ALL_ROOM_TINTS) {
        static_assert(NUM_ROOM_TINTS == 2);
        const auto namedColor = std::invoke([tint]() -> XNamedColor {
            switch (tint) {
            case RoomTintEnum::DARK:
                return LOOKUP_COLOR(ROOM_DARK);
            case RoomTintEnum::NO_SUNDEATH:
                return LOOKUP_COLOR(ROOM_NO_SUNDEATH);
            }
            std::abort();
        });

        if (const auto optColor = getColor(namedColor)) {
            tints[tint].render(equal_multiplied.withColor(optColor.value()));
        } else {
            assert(false);
        }
    }

    if (!disableTextures) {
        batch_lequal_blended_transparent.render(lequal_blended.withColor(color));
        batch_equal_blended_transparent.render(equal_blended.withColor(color));
    }

    if (thisLayer != focusedLayer) {
        const auto baseAlpha = (thisLayer < focusedLayer) ? 0.5f : 0.1f;
        const auto alpha
            = glm::clamp(baseAlpha + 0.03f * static_cast<float>(std::abs(focusedLayer - thisLayer)),
                         0.f,
                         1.f);
        const Color &baseColor = (thisLayer < focusedLayer || disableTextures) ? Colors::black
                                                                               : Colors::white;
        layerBoost.render(equal_blended.withColor(baseColor.withAlpha(alpha)));
    }
}

void InternalData::virt_finish(MapBatches &output, OpenGL &gl, GLFont &font) const
{
    DECL_TIMER(t, "InternalData::virt_finish");

    {
        DECL_TIMER(t2, "InternalData::virt_finish batchedMeshes");
        for (const auto &kv : batchedMeshes) {
            const LayerMeshesIntermediate &data = kv.second;
            output.batchedMeshes[kv.first] = data.getLayerMeshes(gl);
        }
    }
    {
        DECL_TIMER(t2, "InternalData::virt_finish connectionMeshes");
        for (const auto &kv : connectionDrawerBuffers) {
            const ConnectionDrawerBuffers &data = kv.second;
            output.connectionMeshes[kv.first] = data.getMeshes(gl);
        }
    }

    {
        DECL_TIMER(t2, "InternalData::virt_finish roomNameBatches");
        for (const auto &kv : roomNameBatches) {
            const RoomNameBatchIntermediate &rnb = kv.second;
            output.roomNameBatches[kv.first] = rnb.getMesh(font);
        }
    }
}

// NOTE: All of the lamda captures are copied, including the texture data!
FutureSharedMapBatchFinisher generateMapDataFinisher(const mctp::MapCanvasTexturesProxy &textures,
                                                     const std::shared_ptr<const FontMetrics> &font,
                                                     const Map &map)
{
    const auto visitRoomOptions = getVisitRoomOptions();

    return std::async(std::launch::async,
                      [textures, font, map, visitRoomOptions]() -> SharedMapBatchFinisher {
                          ThreadLocalNamedColorRaii tlRaii{visitRoomOptions.canvasColors,
                                                           visitRoomOptions.colorSettings};
                          DECL_TIMER(t, "[ASYNC] generateAllLayerMeshes");

                          ProgressCounter dummyPc;
                          map.checkConsistency(dummyPc);

                          const auto layerToRooms = std::invoke([map]() -> LayerToRooms {
                              DECL_TIMER(t2, "[ASYNC] generateBatches.layerToRooms");
                              LayerToRooms ltr;
                              map.getRooms().for_each([&map, &ltr](const RoomId id) {
                                  const auto &r = map.getRoomHandle(id);
                                  const auto z = r.getPosition().z;
                                  auto &layer = ltr[z];
                                  layer.emplace_back(r);
                              });
                              return ltr;
                          });

                          auto result = std::make_shared<InternalData>();
                          auto &data = deref(result);
                          generateAllLayerMeshes(data,
                                                 deref(font),
                                                 layerToRooms,
                                                 textures,
                                                 visitRoomOptions);
                          return SharedMapBatchFinisher{result};
                      });
}

void finish(const IMapBatchesFinisher &finisher,
            std::optional<MapBatches> &opt_batches,
            OpenGL &gl,
            GLFont &font)
{
    MapBatches &batches = opt_batches.emplace();

    // Note: This will call InternalData::finish;
    // if necessary for claritiy, we could replace this with Pimpl to make it a direct call,
    // but that won't change the cost of the virtual call.
    finisher.finish(batches, gl, font);
}
