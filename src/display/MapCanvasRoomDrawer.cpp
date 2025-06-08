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
#include "ConnectionLineBuilder.h"
#include "MapBatches.h" // For ChunkId
#include "MapCanvasData.h"
#include "RoadIndex.h"
#include "mapcanvas.h" // hack, since we're now definining some of its symbols

#include <cassert>
#include <cstdlib>
#include <functional> // Added for std::hash
#include <memory>
#include <optional>
#include <set>
#include <stdexcept>
#include <unordered_map>
#include <vector>
#include "src/mapdata/remesh_types.h" // For IterativeRemeshMetadata

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <QColor>
#include <QMessageLogContext>
#include <QtGui/qopengl.h>
#include <QtGui>

NODISCARD RoomAreaHash getRoomAreaHash(const RoomHandle& room) {
    const RoomArea& area = room.getArea();
    if (area.empty()) {
        // Handle cases where RoomArea might be empty, if necessary.
        // For now, returning a default RoomAreaHash or a hash of an empty string.
        return 0;
    }
    std::hash<std::string> hasher;
    return static_cast<RoomAreaHash>(hasher(area.toStdStringUtf8()));
}

// All struct definitions RoomTex, ColoredRoomTex, RoomTexVector, ColoredRoomTexVector, LayerBatchData
// including their method implementations (like sortByTexture, getMeshes)
// are being removed from here as their definitions (declarations for methods) are moved to .h
// The method *implementations* remain here, but outside of any struct explicitly defined here.

// RoomTex constructor (implementation)
RoomTex::RoomTex(RoomHandle moved_room, const MMTextureId input_texid)
    : room{std::move(moved_room)}
    , tex{input_texid}
{
    if (input_texid == INVALID_MM_TEXTURE_ID) {
        throw std::invalid_argument("input_texid");
    }
}

// ColoredRoomTex constructor (implementation)
ColoredRoomTex::ColoredRoomTex(RoomHandle moved_room,
                        const MMTextureId input_texid,
                        const Color &input_color)
    : RoomTex{std::move(moved_room), input_texid}
    , color{input_color}
{}

// RoomTexVector methods (implementations)
void RoomTexVector::sortByTexture()
{
    if (size() < 2) {
        return;
    }
    RoomTex *const beg = data();
    RoomTex *const end = beg + size();
    std::sort(beg, end);
}

NODISCARD bool RoomTexVector::isSorted() const
{
    if (size() < 2) {
        return true;
    }
    const RoomTex *const beg = data();
    const RoomTex *const end = beg + size();
    return std::is_sorted(beg, end);
}

// ColoredRoomTexVector methods (implementations)
void ColoredRoomTexVector::sortByTexture()
{
    if (size() < 2) {
        return;
    }
    ColoredRoomTex *const beg = data();
    ColoredRoomTex *const end = beg + size();
    std::sort(beg, end);
}

NODISCARD bool ColoredRoomTexVector::isSorted() const
{
    if (size() < 2) {
        return true;
    }
    const ColoredRoomTex *const beg = data();
    const ColoredRoomTex *const end = beg + size();
    return std::is_sorted(beg, end);
}

// LayerBatchData methods (implementations)
void LayerBatchData::sort()
{
    DECL_TIMER(t, "sort");
    roomTerrains.sortByTexture();
    roomTrails.sortByTexture();
    roomOverlays.sortByTexture();
    roomUpDownExits.sortByTexture();
    doors.sortByTexture();
    solidWallLines.sortByTexture();
    dottedWallLines.sortByTexture();
    streamIns.sortByTexture();
    streamOuts.sortByTexture();
}

NODISCARD LayerMeshes LayerBatchData::getMeshes(OpenGL &gl) const
{
    DECL_TIMER(t, "getMeshes");
    LayerMeshes meshes;
    meshes.terrain = ::createSortedTexturedMeshes("terrain", gl, roomTerrains);
    meshes.trails = ::createSortedTexturedMeshes("trails", gl, roomTrails);
    // meshes.darkTintMesh = gl.createPlainQuadBatch(darkQuads);             // Removed
    // meshes.noSundeathTintMesh = gl.createPlainQuadBatch(noSundeathQuads); // Removed
    for (size_t i = 0; i < NUM_ROOM_TINTS; ++i) {
        if (!roomTints[i].empty()) { // Only create mesh if there are quads
            meshes.tints[i] = gl.createPlainQuadBatch(roomTints[i]);
        } else {
            // Ensure meshes.tints[i] is in a valid, empty state if no quads
            meshes.tints[i] = UniqueMesh{};
        }
    }
    meshes.overlays = ::createSortedTexturedMeshes("overlays", gl, roomOverlays);
    meshes.doors = ::createSortedColoredTexturedMeshes("doors", gl, doors);
    meshes.walls = ::createSortedColoredTexturedMeshes("solidWalls", gl, solidWallLines);
    meshes.dottedWalls = ::createSortedColoredTexturedMeshes("dottedWalls", gl, dottedWallLines);
    meshes.upDownExits = ::createSortedColoredTexturedMeshes("upDownExits", gl, roomUpDownExits);
    meshes.streamIns = ::createSortedColoredTexturedMeshes("streamIns", gl, streamIns);
    meshes.streamOuts = ::createSortedColoredTexturedMeshes("streamOuts", gl, streamOuts);
    meshes.layerBoost = gl.createPlainQuadBatch(roomLayerBoostQuads);
    meshes.isValid = true;
    return meshes;
}

// struct NODISCARD VisitRoomOptions final // Moved to .h
// {
//     SharedCanvasNamedColorOptions canvasColors;
//     SharedNamedColorOptions colorSettings;
//     bool drawNotMappedExits = false;
// };

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
    result.canvasColors = static_cast<const Configuration::CanvasNamedColorOptions &>(canvas).clone();
    result.colorSettings
        = static_cast<const Configuration::NamedColorOptions &>(config.colorSettings).clone();
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
    MMTextureId terrain = INVALID_MM_TEXTURE_ID;
    MMTextureId trail = INVALID_MM_TEXTURE_ID;
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
    virtual ~IRoomVisitorCallbacks();

private:
    NODISCARD virtual bool virt_acceptRoom(const RoomHandle &) const = 0;

private:
    // Rooms
    virtual void virt_visitTerrainTexture(const RoomHandle &, MMTextureId) = 0;
    virtual void virt_visitTrailTexture(const RoomHandle &, MMTextureId) = 0;
    virtual void virt_visitOverlayTexture(const RoomHandle &, MMTextureId) = 0;
    // virtual void virt_visitNamedColorTint(const RoomHandle &, RoomTintEnum) = 0; // Removed
    virtual void virt_addDarkQuad(const RoomHandle &) = 0; // Added
    virtual void virt_addNoSundeathQuad(const RoomHandle &) = 0; // Added

    // Walls
    virtual void virt_visitWall(
        const RoomHandle &, ExitDirEnum, const XNamedColor &, WallTypeEnum, bool isClimb)
        = 0;

    // Streams
    virtual void virt_visitStream(const RoomHandle &, ExitDirEnum, StreamTypeEnum) = 0;

public:
    NODISCARD bool acceptRoom(const RoomHandle &room) const { return virt_acceptRoom(room); }

    // Rooms
    void visitTerrainTexture(const RoomHandle &room, const MMTextureId tex)
    {
        virt_visitTerrainTexture(room, tex);
    }
    void visitTrailTexture(const RoomHandle &room, const MMTextureId tex)
    {
        virt_visitTrailTexture(room, tex);
    }
    void visitOverlayTexture(const RoomHandle &room, const MMTextureId tex)
    {
        virt_visitOverlayTexture(room, tex);
    }
    // void visitNamedColorTint(const RoomHandle &room, const RoomTintEnum tint) // Removed
    // {
    //     virt_visitNamedColorTint(room, tint);
    // }
    void addDarkQuad(const RoomHandle &room) // Added
    {
        virt_addDarkQuad(room);
    }
    void addNoSundeathQuad(const RoomHandle &room) // Added
    {
        virt_addNoSundeathQuad(room);
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

    if (auto trail = terrainAndTrail.trail) {
        callbacks.visitOverlayTexture(room, trail);
    }

    if (isDark) {
        // callbacks.visitNamedColorTint(room, RoomTintEnum::DARK); // Old call
        callbacks.addDarkQuad(room); // New call
    } else if (hasNoSundeath) {
        // callbacks.visitNamedColorTint(room, RoomTintEnum::NO_SUNDEATH); // Old call
        callbacks.addNoSundeathQuad(room); // New call
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
} // <<<< This closes static void visitRoom

// Removed the extra brace here

static void visitRooms(const RoomVector &rooms,
                       const mctp::MapCanvasTexturesProxy &textures,
                       IRoomVisitorCallbacks &callbacks,
                       const VisitRoomOptions &visitRoomOptions)
{
    DECL_TIMER(t, __FUNCTION__);
    for (const auto &room : rooms) {
        visitRoom(room, textures, callbacks, visitRoomOptions);
    }
} // <<<< Added missing closing brace for static void visitRooms

// Removed struct RoomTex - Moved to .h
// Removed struct ColoredRoomTex - Moved to .h
// Removed struct RoomTexVector - Moved to .h
// Removed struct ColoredRoomTexVector - Moved to .h

// template<typename T, typename Callback> static void foreach_texture(...) // Moved to .h
// NODISCARD static UniqueMeshVector createSortedTexturedMeshes(...) // Moved to .h
// NODISCARD static UniqueMeshVector createSortedColoredTexturedMeshes(...) // Moved to .h

// Removed using PlainQuadBatch - Definition is implicit via std::vector<glm::vec3> and includes
// Removed struct LayerBatchData - Moved to .h

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

    void virt_visitTerrainTexture(const RoomHandle &room, const MMTextureId terrain) final
    {
        if (terrain == INVALID_MM_TEXTURE_ID) {
            return;
        }

        m_data.roomTerrains.emplace_back(room, terrain);

        const auto v0 = room.getPosition().to_vec3();
#define EMIT(x, y) m_data.roomLayerBoostQuads.emplace_back(v0 + glm::vec3((x), (y), 0))
        EMIT(0, 0);
        EMIT(1, 0);
        EMIT(1, 1);
        EMIT(0, 1);
#undef EMIT
    }

    void virt_visitTrailTexture(const RoomHandle &room, const MMTextureId trail) final
    {
        if (trail != INVALID_MM_TEXTURE_ID) {
            m_data.roomTrails.emplace_back(room, trail);
        }
    }

    void virt_visitOverlayTexture(const RoomHandle &room, const MMTextureId overlay) final
    {
        if (overlay != INVALID_MM_TEXTURE_ID) {
            m_data.roomOverlays.emplace_back(room, overlay);
        }
    }

    // void virt_visitNamedColorTint(const RoomHandle &room, const RoomTintEnum tint) final // Removed
    // {
    //     const auto v0 = room.getPosition().to_vec3();
    // #define EMIT(x, y) m_data.roomTints[tint].emplace_back(v0 + glm::vec3((x), (y), 0))
    //     EMIT(0, 0);
    //     EMIT(1, 0);
    //     EMIT(1, 1);
    //     EMIT(0, 1);
    // #undef EMIT
    // }

    void virt_addDarkQuad(const RoomHandle &room) final // Added
    {
        const auto v0 = room.getPosition().to_vec3();
        // Add to the correct index in roomTints
        PlainQuadBatch& quad_batch = m_data.roomTints[TintIndices::DARK];
        quad_batch.emplace_back(v0 + glm::vec3(0, 0, 0));
        quad_batch.emplace_back(v0 + glm::vec3(1, 0, 0));
        quad_batch.emplace_back(v0 + glm::vec3(1, 1, 0));
        quad_batch.emplace_back(v0 + glm::vec3(0, 1, 0));
    }

    void virt_addNoSundeathQuad(const RoomHandle &room) final // Added
    {
        const auto v0 = room.getPosition().to_vec3();
        // Add to the correct index in roomTints
        PlainQuadBatch& quad_batch = m_data.roomTints[TintIndices::NO_SUNDEATH];
        quad_batch.emplace_back(v0 + glm::vec3(0, 0, 0));
        quad_batch.emplace_back(v0 + glm::vec3(1, 0, 0));
        quad_batch.emplace_back(v0 + glm::vec3(1, 1, 0));
        quad_batch.emplace_back(v0 + glm::vec3(0, 1, 0));
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

        const auto glcolor = optColor.value();

        if (wallType == WallTypeEnum::DOOR) {
            // Note: We could use two door textures (NESW and UD), and then just rotate the
            // texture coordinates, but doing that would require a different code path.
            const MMTextureId tex = m_textures.door[dir];
            m_data.doors.emplace_back(room, tex, glcolor);
        } else {
            if (isNESW(dir)) {
                if (wallType == WallTypeEnum::SOLID) {
                    const MMTextureId tex = m_textures.wall[dir];
                    m_data.solidWallLines.emplace_back(room, tex, glcolor);
                } else {
                    const MMTextureId tex = m_textures.dotted_wall[dir];
                    m_data.dottedWallLines.emplace_back(room, tex, glcolor);
                }
            } else {
                const bool isUp = dir == ExitDirEnum::UP;
                assert(isUp || dir == ExitDirEnum::DOWN);

                const MMTextureId tex = isClimb
                                            ? (isUp ? m_textures.exit_climb_up
                                                    : m_textures.exit_climb_down)
                                            : (isUp ? m_textures.exit_up : m_textures.exit_down);

                m_data.roomUpDownExits.emplace_back(room, tex, glcolor);
            }
        }
    }

    void virt_visitStream(const RoomHandle &room,
                          const ExitDirEnum dir,
                          const StreamTypeEnum type) final
    {
        const Color color = LOOKUP_COLOR(STREAM).getColor();
        switch (type) {
        case StreamTypeEnum::OutFlow:
            m_data.streamOuts.emplace_back(room, m_textures.stream_out[dir], color);
            return;
        case StreamTypeEnum::InFlow:
            m_data.streamIns.emplace_back(room, m_textures.stream_in[dir], color);
            return;
        default:
            break;
        }

        assert(false);
    }
};

LayerBatchBuilder::~LayerBatchBuilder() = default;

NODISCARD static LayerBatchData generateLayerMeshes(const RoomVector &rooms, // Should be pre-filtered for the chunk
                                                    [[maybe_unused]] RoomAreaHash chunkId,
                                                    const mctp::MapCanvasTexturesProxy &textures,
                                                    const OptBounds &bounds, // This is the overall bounds, not chunk specific.
                                                    const VisitRoomOptions &visitRoomOptions)
{
    DECL_TIMER(t, "generateLayerMeshes");

    // Rooms are now assumed to be pre-filtered for the specific chunkId.
    // No explicit filtering loop is needed here anymore.
    LayerBatchData data;
    LayerBatchBuilder builder{data, textures, bounds};
    visitRooms(rooms, textures, builder, visitRoomOptions);

    data.sort();
    return data;
}

// struct NODISCARD InternalData final : public IMapBatchesFinisher // Moved to .h
// {
// public:
//     std::unordered_map<int, std::map<ChunkId, LayerBatchData>> batchedMeshes;
//     std::map<int, std::map<ChunkId, ConnectionDrawerBuffers>> connectionDrawerBuffers;
//     std::map<int, std::map<ChunkId, RoomNameBatch>> roomNameBatches;
// private:
//     void virt_finish(MapBatches &output, OpenGL &gl, GLFont &font) const final;
// };

static void generateAllLayerMeshes(InternalData &internalData, // Type already InternalData due to previous changes
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
    // connectionDrawerBuffers and roomNameBatches are now chunked, so direct layer access is removed here.
    // auto &connectionDrawerBuffers_layer = internalData.connectionDrawerBuffers; // Old way
    // auto &roomNameBatches_layer = internalData.roomNameBatches; // Old way

    for (const auto &layer_entry : layerToRooms) { // Renamed 'layer' to 'layer_entry' for clarity
        DECL_TIMER(t2, "generateAllLayerMeshes.loop");
        const int thisLayer = layer_entry.first;
        const RoomVector& rooms_in_layer = layer_entry.second;

        // Group rooms by RoomAreaHash for this layer
        std::map<RoomAreaHash, RoomVector> chunkedRoomsOnLayer;
        for (const auto& room : rooms_in_layer) {
            RoomAreaHash roomAreaHash = getRoomAreaHash(room);
            chunkedRoomsOnLayer[roomAreaHash].push_back(room);
        }

        // Process each chunk
        for (const auto& chunk_entry : chunkedRoomsOnLayer) {
            RoomAreaHash currentRoomAreaHash = chunk_entry.first;
            const RoomVector& rooms_for_this_chunk = chunk_entry.second;

            // if (rooms_for_this_chunk.empty()) { // Removed conditional skip
            //     continue;
            // }

            DECL_TIMER(t3, "generateAllLayerMeshes.loop.generateChunkMeshes");
            // Process even if rooms_for_this_chunk is empty
            batchedMeshes[thisLayer][currentRoomAreaHash] =
                ::generateLayerMeshes(rooms_for_this_chunk, currentRoomAreaHash, textures, bounds, visitRoomOptions);

            // Ensure connection and room name batches are at least cleared/default-constructed.
            ConnectionDrawerBuffers& cdb_chunk = internalData.connectionDrawerBuffers[thisLayer][currentRoomAreaHash];
            RoomNameBatch& rnb_chunk = internalData.roomNameBatches[thisLayer][currentRoomAreaHash];
            cdb_chunk.clear();
            rnb_chunk.clear();

            // Only attempt to draw connections/names if there were actual rooms.
            if (!rooms_for_this_chunk.empty()) {
                // The 'bounds' here is still the overall OptBounds passed into generateAllLayerMeshes.
                // ConnectionDrawer will only process rooms_for_this_chunk.
                ConnectionDrawer cd{cdb_chunk, rnb_chunk, thisLayer, bounds};
                for (const auto &room : rooms_for_this_chunk) {
                    cd.drawRoomConnectionsAndDoors(room);
                }
            }
        }
        // Old layer-wide connection/name generation is removed as it's now per-chunk.
    }
}

// Changed first parameter type from IMapBatchesFinisher::InternalData& to InternalData&
void generateSpecificLayerMeshes(InternalData &internalData,
                                 const Map &map,
                                 const std::vector<std::pair<int, RoomAreaHash>>& chunksToGenerate,
                                 const mctp::MapCanvasTexturesProxy &textures,
                                 const VisitRoomOptions &visitRoomOptions)
{
    const OptBounds bounds{}; // Use default OptBounds, similar to generateAllLayerMeshes

    DECL_TIMER(t, "generateSpecificLayerMeshes");

    for (const auto& chunk_info : chunksToGenerate) {
        const int layerId = chunk_info.first;
        const RoomAreaHash roomAreaHash = chunk_info.second;

        RoomVector rooms_for_this_chunk_layer;
        // Collect rooms for the specific layerId and roomAreaHash
        for (const RoomId roomId : map.getRooms()) {
            const auto &room = map.getRoomHandle(roomId);
            if (room.getPosition().z == layerId) {
                if (getRoomAreaHash(room) == roomAreaHash) {
                    rooms_for_this_chunk_layer.push_back(room);
                }
            }
        }

        // if (rooms_for_this_chunk_layer.empty()) { // Removed conditional skip
        //     continue;
        // }

        DECL_TIMER(t_chunk, "generateSpecificLayerMeshes.loop.generateSingleChunkMeshes");
        // Generate meshes for rooms in this specific chunk and layer, even if rooms_for_this_chunk_layer is empty.
        // ::generateLayerMeshes is expected to handle an empty RoomVector and return an empty LayerBatchData.
        internalData.batchedMeshes[layerId][roomAreaHash] =
            ::generateLayerMeshes(rooms_for_this_chunk_layer, roomAreaHash, textures, bounds, visitRoomOptions);

        // Ensure connection and room name batches are at least cleared/default-constructed.
        ConnectionDrawerBuffers& cdb_chunk = internalData.connectionDrawerBuffers[layerId][roomAreaHash];
        RoomNameBatch& rnb_chunk = internalData.roomNameBatches[layerId][roomAreaHash];
        cdb_chunk.clear();
        rnb_chunk.clear();

        // Only attempt to draw connections/names if there were actual rooms.
        if (!rooms_for_this_chunk_layer.empty()) {
            ConnectionDrawer cd{cdb_chunk, rnb_chunk, layerId, bounds};
            for (const auto &room : rooms_for_this_chunk_layer) {
                cd.drawRoomConnectionsAndDoors(room);
            }
        }
    }
}

void LayerMeshes::render(const int thisLayer, const int focusedLayer)
{
    bool disableTextures = false;
    if (thisLayer > focusedLayer) {
        if (!getConfig().canvas.drawUpperLayersTextured) {
            // Disable texturing for this layer. We want to draw
            // all of the squares in white (using layer boost quads),
            // and then still draw the walls.
            disableTextures = true;
        }
    }

    const GLRenderState less = GLRenderState().withDepthFunction(DepthFunctionEnum::LESS);
    const GLRenderState equal = GLRenderState().withDepthFunction(DepthFunctionEnum::EQUAL);
    const GLRenderState lequal = GLRenderState().withDepthFunction(DepthFunctionEnum::LEQUAL);

    const GLRenderState less_blended = less.withBlend(BlendModeEnum::TRANSPARENCY);
    const GLRenderState lequal_blended = lequal.withBlend(BlendModeEnum::TRANSPARENCY);
    const GLRenderState equal_blended = equal.withBlend(BlendModeEnum::TRANSPARENCY);
    const GLRenderState equal_multiplied = equal.withBlend(BlendModeEnum::MODULATE);

    const auto color = [&thisLayer, &focusedLayer]() {
        if (thisLayer <= focusedLayer) {
            return Colors::white.withAlpha(0.90f);
        }
        return Colors::gray70.withAlpha(0.20f);
    }();

    {
        /* REVISIT: For the modern case, we could render each layer separately,
         * and then only blend the layers that actually overlap. Doing that would
         * give higher contrast for the base textures.
         */
        if (disableTextures) {
            const auto layerWhite = Colors::white.withAlpha(0.20f);
            layerBoost.render(less_blended.withColor(layerWhite));
        } else {
            terrain.render(less_blended.withColor(color));
        }
    }

    // Render specific tint meshes (now looped)
    for (size_t i = 0; i < NUM_ROOM_TINTS; ++i) {
        if (tints[i].isValid()) {
            std::optional<Color> optColor;
            if (i == TintIndices::DARK) {
                optColor = getColor(LOOKUP_COLOR(ROOM_DARK));
            } else if (i == TintIndices::NO_SUNDEATH) {
                optColor = getColor(LOOKUP_COLOR(ROOM_NO_SUNDEATH));
            } else {
                // This case implies NUM_ROOM_TINTS might be > 2 or TintIndices are not just 0, 1.
                // For safety with current NUM_ROOM_TINTS = 2, this path might not be hit.
                // If it can be hit due to future changes, ensure optColor remains std::nullopt or handle appropriately.
                continue;
            }

            if (optColor) { // Check if getColor returned a valid color (it wasn't transparent)
                // 'equal_multiplied' should be an existing GLRenderState variable in this function
                tints[i].render(equal_multiplied.withColor(optColor.value()));
            }
        }
    }

    if (!disableTextures) {
        // streams go under everything else, including trails
        streamIns.render(lequal_blended.withColor(color));
        streamOuts.render(lequal_blended.withColor(color));

        trails.render(equal_blended.withColor(color));
        overlays.render(equal_blended.withColor(color));
    }

    // always
    {
        // doors and walls are considered lines, even though they're drawn with textures.
        upDownExits.render(equal_blended.withColor(color));

        // Doors are drawn on top of the up-down exits
        doors.render(lequal_blended.withColor(color));
        // and walls are drawn on top of doors.
        walls.render(lequal_blended.withColor(color));
        dottedWalls.render(lequal_blended.withColor(color));
    }

    if (thisLayer != focusedLayer) {
        // Darker when below, lighter when above
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

OpenDiablo2::Display::FinishedRemeshPayload InternalData::virt_finish(OpenGL &gl, GLFont &font) const
{
    DECL_TIMER(t_overall, "InternalData::virt_finish");
    OpenDiablo2::Display::FinishedRemeshPayload payload;
    payload.generatedMeshes.clear(); // Clear previous batches

    // TODO: Determine m_levelOrLoadedAreaId more accurately.
    // If iterativeState is present and has allTargetChunks, maybe use the level from the first target chunk.
    // Or, if processedChunksThisCall is not empty, use the level from its first element.
    // For now, defaulting to 0 or a common value if determinable.
    payload.generatedMeshes.m_levelOrLoadedAreaId = 0;
    if (this->iterativeState.has_value() && !this->iterativeState.value().allTargetChunks.empty()) {
        payload.generatedMeshes.m_levelOrLoadedAreaId = this->iterativeState.value().allTargetChunks.front().first;
    } else if (!this->processedChunksThisCall.empty()) {
        payload.generatedMeshes.m_levelOrLoadedAreaId = this->processedChunksThisCall.front().first;
    }

    payload.generatedMeshes.m_generatedFromSpecificChunks = true;

    for (const auto &layer_kv : this->batchedMeshes) {
        const int layerId = layer_kv.first;
        for (const auto &chunk_kv : layer_kv.second) {
            const RoomAreaHash areaHash = chunk_kv.first;
            const LayerBatchData &batch_data = chunk_kv.second;
            payload.generatedMeshes.batchedMeshes[layerId].insert_or_assign(areaHash, batch_data.getMeshes(gl));
        }
    }

    for (const auto &layer_chunk_buffers_pair : this->connectionDrawerBuffers) {
        const int layerId = layer_chunk_buffers_pair.first;
        const auto& chunk_buffers_map = layer_chunk_buffers_pair.second;
        for (const auto& chunk_buffer_pair : chunk_buffers_map) {
            const RoomAreaHash roomAreaHash = chunk_buffer_pair.first;
            const ConnectionDrawerBuffers& cdb_data = chunk_buffer_pair.second;
            payload.generatedMeshes.connectionMeshes[layerId].insert_or_assign(roomAreaHash, cdb_data.getMeshes(gl));
        }
    }

    for (const auto &layer_chunk_rnb_pair : this->roomNameBatches) {
        const int layerId = layer_chunk_rnb_pair.first;
        const auto& chunk_rnb_map = layer_chunk_rnb_pair.second;
        for (const auto& chunk_rnb_pair : chunk_rnb_map) {
            const RoomAreaHash roomAreaHash = chunk_rnb_pair.first;
            const RoomNameBatch& rnb_data = chunk_rnb_pair.second;
            // Assuming getMesh for RoomNameBatch takes GLFont& font, not OpenGL& gl, GLFont& font
            payload.generatedMeshes.roomNameBatches[layerId].insert_or_assign(roomAreaHash, rnb_data.getMesh(gl, font));
        }
    }
    // Infomark batches are typically global and not handled per chunk here.

    payload.chunksCompletedThisPass = this->processedChunksThisCall;

    // Strategy has been removed from IterativeRemeshMetadata, so the check is only for its presence.
    // The logic within correctly handles iterative passes if iterativeState is present.
    if (this->iterativeState.has_value()) {
        // Assuming this->iterativeState is already OpenDiablo2::Display::IterativeRemeshMetadata
        // If not, this line and its usage would need type adjustment.
        // For now, proceeding with the assumption it's the new type or will be updated by other steps.
        OpenDiablo2::Display::IterativeRemeshMetadata nextState = this->iterativeState.value();
        for (const auto& chunkProfile : payload.chunksCompletedThisPass) {
            nextState.completedChunks.insert(chunkProfile);
        }
        nextState.currentPassNumber++;

        bool allDone = true;
        // Strategy check removed from here as well.
        if (nextState.allTargetChunks.empty()) {
            // If allTargetChunks is empty, consider it done.
             allDone = true;
        } else {
            for (const auto& targetChunkPair : nextState.allTargetChunks) {
                if (nextState.completedChunks.find(targetChunkPair) == nextState.completedChunks.end()) {
                    allDone = false;
                    break;
                }
            }
        }


        if (allDone) {
            payload.morePassesNeeded = false;
            payload.nextIterativeState = std::nullopt;
        } else {
            payload.morePassesNeeded = true;
            payload.nextIterativeState = nextState;
        }
    } else {
        // This branch handles:
        // 1. No iterativeState present (e.g., old non-iterative calls or generateMapDataFinisher)
        // 2. IterativeState present but strategy is AllAtOnce (which should complete in one pass)
        payload.morePassesNeeded = false;
        payload.nextIterativeState = std::nullopt;
    }

    return payload;
}

// NOTE: All of the lamda captures are copied, including the texture data!
FutureSharedMapBatchFinisher generateMapDataFinisher(const mctp::MapCanvasTexturesProxy &textures,
                                                     const Map &map)
{
    const auto visitRoomOptions = getVisitRoomOptions();

    return std::async(std::launch::async,
                      [textures, map, visitRoomOptions]() -> SharedMapBatchFinisher {
                          ThreadLocalNamedColorRaii tlRaii{visitRoomOptions.canvasColors,
                                                           visitRoomOptions.colorSettings};
                          DECL_TIMER(t, "[ASYNC] generateAllLayerMeshes");

                          const LayerToRooms layerToRooms = [map]() -> LayerToRooms {
                              DECL_TIMER(t2, "[ASYNC] generateBatches.layerToRooms");
                              LayerToRooms ltr;
                              for (const RoomId id : map.getRooms()) {
                                  const auto &r = map.getRoomHandle(id);
                                  const auto z = r.getPosition().z;
                                  auto &layer = ltr[z];
                                  layer.emplace_back(r);
                              }
                              return ltr;
                          }();
                          // For generateMapDataFinisher (non-specific chunks), iterativeState is not applicable in the same way.
                          // We pass map and calibratedWorldHalfLineWidth to the constructor.
                          auto result = std::make_shared<InternalData>(map, textures.getCalibratedWorldHalfLineWidth());
                          // auto &data = deref(result); // No longer needed
                          generateAllLayerMeshes(*result, layerToRooms, textures, visitRoomOptions);
                          return SharedMapBatchFinisher{result};
                      });
}

FutureSharedMapBatchFinisher
generateSpecificMapDataFinisher(
    const mctp::MapCanvasTexturesProxy &textures,
    const Map &map,
    const std::vector<std::pair<int, RoomAreaHash>>& chunksToGenerateThisPass,
    std::optional<OpenDiablo2::MapData::IterativeRemeshMetadata> currentIterativeState)
{
    const auto visitRoomOptions = getVisitRoomOptions();

    // Ensure chunksToGenerateThisPass and currentIterativeState are moved/copied into the lambda
    return std::async(std::launch::async,
                      [textures, map, visitRoomOptions, chunksToGenerate = chunksToGenerateThisPass, iterState = std::move(currentIterativeState)]() -> SharedMapBatchFinisher {
                          ThreadLocalNamedColorRaii tlRaii{visitRoomOptions.canvasColors,
                                                           visitRoomOptions.colorSettings};
                          DECL_TIMER(t, "[ASYNC] generateSpecificLayerMeshes");

                          // auto result = std::make_shared<InternalData>(); // Old way
                          // Need to pass map and calibratedWorldHalfLineWidth to InternalData constructor
                          auto result = std::make_shared<InternalData>(map, textures.getCalibratedWorldHalfLineWidth());
                          result->iterativeState = iterState;
                          result->processedChunksThisCall = chunksToGenerate;

                          // auto &data = deref(result); // No longer needed with direct member access
                          generateSpecificLayerMeshes(*result, map, chunksToGenerate, textures, visitRoomOptions);
                          return SharedMapBatchFinisher{result};
                      });
}

void finish(const IMapBatchesFinisher &finisher,
            std::optional<MapBatches> &opt_batches,
            OpenGL &gl,
            GLFont &font)
{
    opt_batches.reset();
    MapBatches &batches = opt_batches.emplace();

    // Note: This will call InternalData::finish;
    // if necessary for claritiy, we could replace this with Pimpl to make it a direct call,
    // but that won't change the cost of the virtual call.
    finisher.finish(batches, gl, font);
}
