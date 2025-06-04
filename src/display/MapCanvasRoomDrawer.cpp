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

    if (auto trail = terrainAndTrail.trail) {
        callbacks.visitOverlayTexture(room, trail);
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
    MMTextureId tex = INVALID_MM_TEXTURE_ID;

    explicit RoomTex(RoomHandle moved_room, const MMTextureId input_texid)
        : room{std::move(moved_room)}
        , tex{input_texid}
    {
        if (input_texid == INVALID_MM_TEXTURE_ID) {
            throw std::invalid_argument("input_texid");
        }
    }

    NODISCARD MMTextureId priority() const { return tex; }
    NODISCARD MMTextureId textureId() const { return tex; }

    NODISCARD friend bool operator<(const RoomTex &lhs, const RoomTex &rhs)
    {
        // true if lhs comes strictly before rhs
        return lhs.priority() < rhs.priority();
    }
};

struct NODISCARD ColoredRoomTex : public RoomTex
{
    Color color;
    ColoredRoomTex(const RoomHandle &room, const MMTextureId tex) = delete;

    explicit ColoredRoomTex(RoomHandle moved_room,
                            const MMTextureId input_texid,
                            const Color &input_color)
        : RoomTex{std::move(moved_room), input_texid}
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
struct NODISCARD RoomTexVector final : public std::vector<RoomTex>
{
    void removeRooms(const RoomIdSet& idsToRemove) {
        this->erase(std::remove_if(this->begin(), this->end(),
                                   [&](const RoomTex& item) {
                                       return idsToRemove.count(item.room.getId());
                                   }),
                    this->end());
    }

    void append(const RoomTexVector& other) {
        this->insert(this->end(), other.begin(), other.end());
        // For moving elements if appropriate (and if `other` can be moved from):
        // this->insert(this->end(), std::make_move_iterator(other.begin()), std::make_move_iterator(other.end()));
    }

    // sorting stl iterators is slower than christmas with GLIBCXX_DEBUG,
    // so we'll use pointers instead. std::stable_sort isn't
    // necessary because everything's opaque, so we'll use
    // vanilla std::sort, which is N log N instead of N^2 log N.
    void sortByTexture()
    {
        if (size() < 2) {
            return;
        }

        RoomTex *const beg = data();
        RoomTex *const end = beg + size();
        // NOTE: comparison will be std::less<RoomTex>, which uses
        // operator<() if it exists.
        std::sort(beg, end);
    }

    NODISCARD bool isSorted() const
    {
        if (size() < 2) {
            return true;
        }

        const RoomTex *const beg = data();
        const RoomTex *const end = beg + size();
        return std::is_sorted(beg, end);
    }
};

struct NODISCARD ColoredRoomTexVector final : public std::vector<ColoredRoomTex>
{
    void removeRooms(const RoomIdSet& idsToRemove) {
        this->erase(std::remove_if(this->begin(), this->end(),
                                   [&](const ColoredRoomTex& item) {
                                       // RoomTex is the base class of ColoredRoomTex and has the 'room' member
                                       return idsToRemove.count(item.room.getId());
                                   }),
                    this->end());
    }

    void append(const ColoredRoomTexVector& other) {
        this->insert(this->end(), other.begin(), other.end());
        // For moving elements if appropriate:
        // this->insert(this->end(), std::make_move_iterator(other.begin()), std::make_move_iterator(other.end()));
    }

    // sorting stl iterators is slower than christmas with GLIBCXX_DEBUG,
    // so we'll use pointers instead. std::stable_sort isn't
    // necessary because everything's opaque, so we'll use
    // vanilla std::sort, which is N log N instead of N^2 log N.
    void sortByTexture()
    {
        if (size() < 2) {
            return;
        }

        ColoredRoomTex *const beg = data();
        ColoredRoomTex *const end = beg + size();
        // NOTE: comparison will be std::less<RoomTex>, which uses
        // operator<() if it exists.
        std::sort(beg, end);
    }

    NODISCARD bool isSorted() const
    {
        if (size() < 2) {
            return true;
        }

        const ColoredRoomTex *const beg = data();
        const ColoredRoomTex *const end = beg + size();
        return std::is_sorted(beg, end);
    }
};

template<typename T, typename Callback>
static void foreach_texture(const T &textures, Callback &&callback)
{
    assert(textures.isSorted());

    const auto size = textures.size();
    for (size_t beg = 0, next = size; beg < size; beg = next) {
        const RoomTex &rtex = textures[beg];
        const auto textureId = rtex.textureId();

        size_t end = beg + 1;
        for (; end < size; ++end) {
            if (textureId != textures[end].textureId()) {
                break;
            }
        }

        next = end;
        /* note: casting to avoid passing references to beg and end */
        callback(static_cast<size_t>(beg), static_cast<size_t>(end));
    }
}

NODISCARD static UniqueMeshVector createSortedTexturedMeshes(const std::string_view /*what*/,
                                                             OpenGL &gl,
                                                             const RoomTexVector &textures)
{
    if (textures.empty()) {
        return UniqueMeshVector{};
    }

    const size_t numUniqueTextures = [&textures]() -> size_t {
        size_t texCount = 0;
        ::foreach_texture(textures,
                          [&texCount](size_t /* beg */, size_t /*end*/) -> void { ++texCount; });
        return texCount;
    }();

    std::vector<UniqueMesh> result_meshes;
    result_meshes.reserve(numUniqueTextures);
    const auto lambda = [&gl, &result_meshes, &textures](const size_t beg,
                                                         const size_t end) -> void {
        const RoomTex &rtex = textures[beg];
        const size_t count = end - beg;

        std::vector<TexVert> verts;
        verts.reserve(count * VERTS_PER_QUAD); /* quads */

        // D-C
        // | |  ccw winding
        // A-B
        for (size_t i = beg; i < end; ++i) {
            const auto &pos = textures[i].room.getPosition();
            const auto v0 = pos.to_vec3();
#define EMIT(x, y) verts.emplace_back(glm::vec2((x), (y)), v0 + glm::vec3((x), (y), 0))
            EMIT(0, 0);
            EMIT(1, 0);
            EMIT(1, 1);
            EMIT(0, 1);
#undef EMIT
        }

        result_meshes.emplace_back(gl.createTexturedQuadBatch(verts, rtex.tex));
    };

    ::foreach_texture(textures, lambda);
    assert(result_meshes.size() == numUniqueTextures);
    return UniqueMeshVector{std::move(result_meshes)};
}

NODISCARD static UniqueMeshVector createSortedColoredTexturedMeshes(
    const std::string_view /*what*/, OpenGL &gl, const ColoredRoomTexVector &textures)
{
    if (textures.empty()) {
        return UniqueMeshVector{};
    }

    const size_t numUniqueTextures = [&textures]() -> size_t {
        size_t texCount = 0;
        ::foreach_texture(textures,
                          [&texCount](size_t /* beg */, size_t /*end*/) -> void { ++texCount; });
        return texCount;
    }();

    std::vector<UniqueMesh> result_meshes;
    result_meshes.reserve(numUniqueTextures);

    const auto lambda = [&gl, &result_meshes, &textures](const size_t beg,
                                                         const size_t end) -> void {
        const RoomTex &rtex = textures[beg];
        const size_t count = end - beg;

        std::vector<ColoredTexVert> verts;
        verts.reserve(count * VERTS_PER_QUAD); /* quads */

        // D-C
        // | |  ccw winding
        // A-B
        for (size_t i = beg; i < end; ++i) {
            const ColoredRoomTex &thisVert = textures[i];
            const auto &pos = thisVert.room.getPosition();
            const auto v0 = pos.to_vec3();
            const auto color = thisVert.color;

#define EMIT(x, y) verts.emplace_back(color, glm::vec2((x), (y)), v0 + glm::vec3((x), (y), 0))
            EMIT(0, 0);
            EMIT(1, 0);
            EMIT(1, 1);
            EMIT(0, 1);
#undef EMIT
        }

        result_meshes.emplace_back(gl.createColoredTexturedQuadBatch(verts, rtex.tex));
    };

    ::foreach_texture(textures, lambda);
    assert(result_meshes.size() == numUniqueTextures);
    return UniqueMeshVector{std::move(result_meshes)};
}

using PlainQuadBatch = std::vector<glm::vec3>;

struct NODISCARD LayerBatchData final
{
    RoomTexVector roomTerrains;
    RoomTexVector roomTrails;
    RoomTexVector roomOverlays;
    // REVISIT: Consider storing up/down door lines in a separate batch,
    // so they can be rendered thicker.
    ColoredRoomTexVector doors;
    ColoredRoomTexVector solidWallLines;
    ColoredRoomTexVector dottedWallLines;
    ColoredRoomTexVector roomUpDownExits;
    ColoredRoomTexVector streamIns;
    ColoredRoomTexVector streamOuts;
    RoomTintArray<PlainQuadBatch> roomTints;
    PlainQuadBatch roomLayerBoostQuads;

    // Methods for partial updates
    void removeRooms(const RoomIdSet& idsToRemove) {
        roomTerrains.removeRooms(idsToRemove);
        roomTrails.removeRooms(idsToRemove);
        roomOverlays.removeRooms(idsToRemove);
        doors.removeRooms(idsToRemove);
        solidWallLines.removeRooms(idsToRemove);
        dottedWallLines.removeRooms(idsToRemove);
        roomUpDownExits.removeRooms(idsToRemove);
        streamIns.removeRooms(idsToRemove);
        streamOuts.removeRooms(idsToRemove);

        // Clear PlainQuadBatch members as per instruction for removeRooms.
        // These will be rebuilt/re-appended based on the rooms remaining or added.
        for (auto& quad_batch : roomTints) { // roomTints is EnumIndexedArray
            quad_batch.clear();
        }
        roomLayerBoostQuads.clear();
    }

    void append(const LayerBatchData& other) {
        roomTerrains.append(other.roomTerrains);
        roomTrails.append(other.roomTrails);
        roomOverlays.append(other.roomOverlays);
        doors.append(other.doors);
        solidWallLines.append(other.solidWallLines);
        dottedWallLines.append(other.dottedWallLines);
        roomUpDownExits.append(other.roomUpDownExits);
        streamIns.append(other.streamIns);
        streamOuts.append(other.streamOuts);

        // Appending PlainQuadBatch for tints and layer boost
        for (const auto tint_idx : ALL_ROOM_TINTS) {
            roomTints[tint_idx].insert(roomTints[tint_idx].end(),
                                       other.roomTints[tint_idx].begin(),
                                       other.roomTints[tint_idx].end());
        }
        roomLayerBoostQuads.insert(roomLayerBoostQuads.end(),
                                   other.roomLayerBoostQuads.begin(),
                                   other.roomLayerBoostQuads.end());
    }

    void sortAllComponents() { // Renamed from sort() to be more specific, or could just be sort()
        DECL_TIMER(t, "sortAllComponents");

        /* TODO: Only sort on 2.1 path, since 3.0 can use GL_TEXTURE_2D_ARRAY. */
        roomTerrains.sortByTexture();
        roomTrails.sortByTexture();
        roomOverlays.sortByTexture();
        doors.sortByTexture();
        solidWallLines.sortByTexture();
        dottedWallLines.sortByTexture();
        roomUpDownExits.sortByTexture();
        streamIns.sortByTexture();
        streamOuts.sortByTexture();
        // No sort needed for PlainQuadBatch members (roomTints, roomLayerBoostQuads)
    }


    explicit LayerBatchData() = default;
    ~LayerBatchData() = default;
    DEFAULT_MOVES(LayerBatchData);
    DELETE_COPIES(LayerBatchData);

    // Original sort method, now effectively sortAllComponents
    void sort() // Keep original sort name if preferred, or use sortAllComponents internally/externally
    {
        sortAllComponents();
    }

    // Note: visitRoomOptions is needed here to correctly determine tint colors based on current settings.
    void regenerateTintsAndBoost(const Map& map /*, const VisitRoomOptions& visitRoomOptions */) {
        // Clear existing tints and boost quads
        for (auto& quad_batch : roomTints) {
            quad_batch.clear();
        }
        roomLayerBoostQuads.clear();

        // This logic is derived from LayerBatchBuilder::virt_visitTerrainTexture and ::virt_visitNamedColorTint
        // We iterate over one of the primary RoomTexVector members to get the rooms in this batch.
        // roomTerrains is a good candidate as all rooms should have terrain.
        for (const RoomTex& rtex : roomTerrains) {
            const RoomHandle& room = rtex.room; // RoomHandle from RoomTex
            // const RawRoom& rawRoom = map.getRoomById(room.getId()); // If direct RawRoom access is needed and not on RoomHandle

            // Add to roomLayerBoostQuads for every room
            const auto v0_boost = room.getPosition().to_vec3();
            #define EMIT_BOOST(x, y) roomLayerBoostQuads.emplace_back(v0_boost + glm::vec3((x), (y), 0))
            EMIT_BOOST(0, 0); EMIT_BOOST(1, 0); EMIT_BOOST(1, 1); EMIT_BOOST(0, 1);
            #undef EMIT_BOOST

            // Determine and add tints
            // This logic mirrors what's in visitRoom -> LayerBatchBuilder callbacks
            // This logic is from LayerBatchBuilder::virt_visitNamedColorTint
            // We need to ensure that RoomHandle provides these methods or we use the map.
            // For this example, assume RoomHandle has getLightType and getSundeathType.
            const bool isDark = room.getLightType() == RoomLightEnum::DARK;
            const bool hasNoSundeath = room.getSundeathType() == RoomSundeathEnum::NO_SUNDEATH;

            if (isDark) {
                const auto v0_tint = room.getPosition().to_vec3();
                #define EMIT_DARK_TINT(x, y) roomTints[RoomTintEnum::DARK].emplace_back(v0_tint + glm::vec3((x), (y), 0))
                EMIT_DARK_TINT(0, 0); EMIT_DARK_TINT(1, 0); EMIT_DARK_TINT(1, 1); EMIT_DARK_TINT(0, 1);
                #undef EMIT_DARK_TINT
            } else if (hasNoSundeath) {
                const auto v0_tint = room.getPosition().to_vec3();
                #define EMIT_NOSUN_TINT(x, y) roomTints[RoomTintEnum::NO_SUNDEATH].emplace_back(v0_tint + glm::vec3((x), (y), 0))
                EMIT_NOSUN_TINT(0, 0); EMIT_NOSUN_TINT(1, 0); EMIT_NOSUN_TINT(1, 1); EMIT_NOSUN_TINT(0, 1);
                #undef EMIT_NOSUN_TINT
            }
            // If neither, no specific tint quad is added for this room, matching original logic.
        }
    }

    NODISCARD LayerMeshes getMeshes(OpenGL &gl) const
    {
        DECL_TIMER(t, "getMeshes");

        LayerMeshes meshes;
        meshes.terrain = ::createSortedTexturedMeshes("terrain", gl, roomTerrains);
        meshes.trails = ::createSortedTexturedMeshes("trails", gl, roomTrails);
        // REVISIT: Can tints be combined now?
        for (const RoomTintEnum tint : ALL_ROOM_TINTS) {
            meshes.tints[tint] = gl.createPlainQuadBatch(roomTints[tint]);
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

NODISCARD static LayerBatchData generateLayerMeshes(const RoomVector &rooms,
                                                    const mctp::MapCanvasTexturesProxy &textures,
                                                    const OptBounds &bounds,
                                                    const VisitRoomOptions &visitRoomOptions)
{
    DECL_TIMER(t, "generateLayerMeshes");

    LayerBatchData data;
    LayerBatchBuilder builder{data, textures, bounds};
    visitRooms(rooms, textures, builder, visitRoomOptions);

    data.sort();
    return data;
}

struct InternalData final : public IMapBatchesFinisher // Ensure this name is consistent with MapCanvasRoomDrawer.h if defined there too
{
public:
    std::unordered_map<int, LayerBatchData> batchedMeshes; // Holds data for the current (potentially partial) update
    BatchedConnections connectionDrawerBuffers; // Holds data for the current (potentially partial) update
    std::unordered_map<int, RoomNameBatch> roomNameBatches; // Holds data for the current (potentially partial) update

    // New member for master data, intended for future merge logic.
    std::unordered_map<int, LayerBatchData> m_masterLayerBatchData;

    // Constructor to initialize with data from the async task
    InternalData(std::unordered_map<int, LayerBatchData> currentBatches,
                 BatchedConnections currentConnectionBuffers, // Ensure this type matches definition if BatchedConnections is not std::map
                 std::unordered_map<int, RoomNameBatch> currentRoomNameBatches)
        : batchedMeshes(std::move(currentBatches)),
          connectionDrawerBuffers(std::move(currentConnectionBuffers)),
          roomNameBatches(std::move(currentRoomNameBatches))
    {}
    // Default constructor might be useful if it's constructed then populated
    InternalData() = default;
InternalData::~InternalData() = default; // Add default destructor implementation


    void virt_finish(
        MapBatches &output_target_on_canvas,
        OpenGL &gl,
        GLFont &font,
        const std::optional<RoomIdSet>& roomsJustUpdated) const override; // Added override and new signature
};

// Forward declare the modified generateAllLayerMeshes_impl which will be defined later
static void generateAllLayerMeshes_impl(
    std::unordered_map<int, LayerBatchData>& outLayerBatches,
    BatchedConnections& outConnectionBuffers,
    std::unordered_map<int, RoomNameBatch>& outRoomNameBatches,
    const LayerToRooms &layerToRooms,
    const mctp::MapCanvasTexturesProxy &textures,
    const VisitRoomOptions &visitRoomOptions,
    const std::optional<RoomIdSet>& roomIdsToProcess);


// This is the new implementation that filters based on roomIdsToProcess
// and populates the provided output parameters.
static void generateAllLayerMeshes_impl(
    std::unordered_map<int, LayerBatchData>& outLayerBatches,
    BatchedConnections& outConnectionBuffers, // Assuming BatchedConnections is std::map<int, ConnectionDrawerBuffers> or similar
    std::unordered_map<int, RoomNameBatch>& outRoomNameBatches,
    const LayerToRooms &layerToRooms,
    const mctp::MapCanvasTexturesProxy &textures,
    const VisitRoomOptions &visitRoomOptions,
    const std::optional<RoomIdSet>& roomIdsToProcess)

{
    // This feature has been removed, but it's passed to a lot of functions,
    // so it would be annoying to have to add it back if we decide the feature was
    // actually necessary.
    const OptBounds bounds{};

    DECL_TIMER(t, "generateAllLayerMeshes");
    // auto& batchedMeshes = internalData.batchedMeshes; // Now using outLayerBatches
    // auto& connectionDrawerBuffers = internalData.connectionDrawerBuffers; // Now using outConnectionBuffers
    // auto& roomNameBatches = internalData.roomNameBatches; // Now using outRoomNameBatches

    for (const auto &layer : layerToRooms) {
        DECL_TIMER(t2, "generateAllLayerMeshes_impl.loop");
        const int thisLayer = layer.first;

        const RoomVector& roomsInLayer = layer.second;
        RoomVector filteredRooms;

        if (roomIdsToProcess) {
            for (const auto& roomHandle : roomsInLayer) {
                if (roomIdsToProcess->count(roomHandle.getId())) {
                    filteredRooms.push_back(roomHandle);
                }
            }
            // If roomIdsToProcess is set and no rooms for this layer are in the set,
            // filteredRooms will be empty. generateLayerMeshes will produce an empty LayerBatchData.
            // If this layer is not mentioned in roomIdsToProcess at all (e.g. partial update for other layers)
            // we might not even want to generate empty data, unless it's to clear previous state.
            // The current logic processes layers present in layerToRooms; if a layer has no rooms matching
            // roomIdsToProcess, it gets empty data.
            if (filteredRooms.empty() && !roomIdsToProcess->empty()) {
                // If we are specifically processing rooms and none are in this layer,
                // but we are doing a partial update (roomIdsToProcess is not empty),
                // we might still want to generate an empty batch to signify this layer was processed (and is now empty for these rooms).
                // Or, if this layer wasn't touched by roomIdsToProcess, we could skip it entirely.
                // For now, if filteredRooms is empty, it will generate empty LayerBatchData.
            }
        } else {
            filteredRooms = roomsInLayer; // Process all rooms in the layer
        }

        // Ensures that even if filteredRooms is empty (e.g. for a partial update not touching this layer,
        // or this layer having no rooms matching the filter), the layer is still represented in the output maps,
        // potentially with empty data. This is important if virt_finish expects all layers.
        // However, if roomIdsToProcess is specified and this layer has no relevant rooms,
        // perhaps it shouldn't be in the output of *this specific partial update*.
        // The current logic will put empty LayerBatchData if filteredRooms is empty.

        LayerBatchData currentLayerBatchData = ::generateLayerMeshes(filteredRooms, textures, bounds, visitRoomOptions);

        ConnectionDrawerBuffers currentConnectionDrawerBuffers_for_layer;
        RoomNameBatch currentRoomNameBatch_for_layer;
        ConnectionDrawer cd{currentConnectionDrawerBuffers_for_layer, currentRoomNameBatch_for_layer, thisLayer, bounds};
        for (const auto &room : filteredRooms) { // Iterate over filtered rooms
            cd.drawRoomConnectionsAndDoors(room);
        }

        // Store the generated (possibly empty) data for this layer into the output maps
        outLayerBatches[thisLayer] = std::move(currentLayerBatchData);
        outConnectionBuffers[thisLayer] = std::move(currentConnectionDrawerBuffers_for_layer);
        outRoomNameBatches[thisLayer] = std::move(currentRoomNameBatch_for_layer);
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

    // REVISIT: move trails to their own batch also colored by the tint?
    for (const RoomTintEnum tint : ALL_ROOM_TINTS) {
        static_assert(NUM_ROOM_TINTS == 2);
        const auto namedColor = [tint]() -> XNamedColor {
            switch (tint) {
            case RoomTintEnum::DARK:
                return LOOKUP_COLOR(ROOM_DARK);
            case RoomTintEnum::NO_SUNDEATH:
                return LOOKUP_COLOR(ROOM_NO_SUNDEATH);
            }
            std::abort();
        }();

        if (const auto optColor = getColor(namedColor)) {
            tints[tint].render(equal_multiplied.withColor(optColor.value()));
        } else {
            assert(false);
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

// This is the implementation of virt_finish for the InternalData struct
void InternalData::virt_finish(
    MapBatches &output_target_on_canvas,
    OpenGL &gl,
    GLFont &font,
    const std::optional<RoomIdSet>& roomsJustUpdated) // Is non-const
{
    // Assuming this is called on the main thread where getVisitRoomOptions() is safe.
    const auto visitRoomOptions = getVisitRoomOptions();

    if (roomsJustUpdated) { // Partial update
        MMLOG_DEBUG() << "InternalData::virt_finish: Partial update for " << roomsJustUpdated->size() << " rooms.";

        for (const auto& layer_kvp : this->batchedMeshes) { // `this->batchedMeshes` contains the incoming partial data
            int layerZ = layer_kvp.first;
            const LayerBatchData& incomingPartialRoomMeshData = layer_kvp.second;
            LayerBatchData& masterLayerRoomMeshData = m_masterLayerBatchData[layerZ];

            masterLayerRoomMeshData.removeRooms(*roomsJustUpdated);
            masterLayerRoomMeshData.append(incomingPartialRoomMeshData);
            masterLayerRoomMeshData.sortAllComponents();
            // Regenerate tints and boosts based on the now updated room list in masterLayerRoomMeshData
            masterLayerRoomMeshData.regenerateTintsAndBoost(m_map /*, visitRoomOptions */); // Pass map, visitRoomOptions if needed by regenerate

            output_target_on_canvas.batchedMeshes[layerZ] = masterLayerRoomMeshData.getMeshes(gl);
        }

        // For connections and room names:
        // For this subtask, continue using direct application of incoming partial data.
        // A full solution would merge into m_masterConnectionDrawerBuffers / m_masterRoomNameBatches
        // and then rebuild the relevant parts of output_target_on_canvas from those master copies.
        for (const auto& kv : this->connectionDrawerBuffers) {
            output_target_on_canvas.connectionMeshes[kv.first] = kv.second.getMeshes(gl);
        }
        for (const auto& kv : this->roomNameBatches) {
            output_target_on_canvas.roomNameBatches[kv.first] = kv.second.getMesh(font);
        }
    } else { // Full update
        MMLOG_DEBUG() << "InternalData::virt_finish: Full update.";
        m_masterLayerBatchData.clear();
        // For full update, `this->batchedMeshes` contains all data. Copy it to master.
        for (const auto& layer_kvp : this->batchedMeshes) {
            m_masterLayerBatchData[layer_kvp.first] = layer_kvp.second; // Assumes LayerBatchData is copy-assignable
        }

        // For connections and room names, also treat 'this->' as the full source data for master copy
        // (Future: these might also have their own master copies if complex merging is needed)
        // m_masterConnectionDrawerBuffers = this->connectionDrawerBuffers;
        // m_masterRoomNameBatches = this->roomNameBatches;

        output_target_on_canvas.batchedMeshes.clear();
        for (const auto& master_layer_kvp : m_masterLayerBatchData) {
            output_target_on_canvas.batchedMeshes[master_layer_kvp.first] = master_layer_kvp.second.getMeshes(gl);
        }

        output_target_on_canvas.connectionMeshes.clear();
        for (const auto &kv : this->connectionDrawerBuffers) { // Still using 'this->' as master for these
            output_target_on_canvas.connectionMeshes[kv.first] = kv.second.getMeshes(gl);
        }
        output_target_on_canvas.roomNameBatches.clear();
        for (const auto &kv : this->roomNameBatches) { // Still using 'this->' as master
            output_target_on_canvas.roomNameBatches[kv.first] = kv.second.getMesh(font);
        }
    }
    // The final loops that unconditionally drew from this->batchedMeshes are removed,
    // as the correct data (either merged master or full new master) is now in output_target_on_canvas.
    MMLOG_DEBUG() << "InternalData::virt_finish completed. roomsJustUpdated: " << (roomsJustUpdated.has_value() ? "Yes" : "No");
}

// NOTE: All of the lamda captures are copied, including the texture data!
FutureSharedMapBatchFinisher generateMapDataFinisher(const mctp::MapCanvasTexturesProxy &textures,
                                                     const Map &map,
                                                     const std::optional<RoomIdSet>& roomIdsToProcess) // Added roomIdsToProcess
{
    const auto visitRoomOptions = getVisitRoomOptions();

    return std::async(std::launch::async,
                      // Capture roomIdsToProcess
                      [textures, map, visitRoomOptions, roomIdsToProcess]() -> SharedMapBatchFinisher {
                          ThreadLocalNamedColorRaii tlRaii{visitRoomOptions.canvasColors,
                                                           visitRoomOptions.colorSettings};
                          DECL_TIMER(t, "[ASYNC] generateMapDataFinisher");

                          const LayerToRooms layerToRooms = [&map]() -> LayerToRooms { // map by ref
                              DECL_TIMER(t2, "[ASYNC] generateBatches.layerToRooms");
                              LayerToRooms ltr;
                              for (const RoomId id : map.getRooms()) { // Use map directly
                                  const auto &r = map.getRoomHandle(id); // Use map directly
                                  const auto z = r.getPosition().z;
                                  auto &layer = ltr[z];
                                  layer.emplace_back(r);
                              }
                              return ltr;
                          }();

                          std::unordered_map<int, LayerBatchData> layerBatches;
                          BatchedConnections connectionBuffers;
                          std::unordered_map<int, RoomNameBatch> roomNameBatchesMap;

                          generateAllLayerMeshes_impl(layerBatches, connectionBuffers, roomNameBatchesMap,
                                                   layerToRooms, textures, visitRoomOptions, roomIdsToProcess);

                          auto result = std::make_shared<InternalData>(
                              std::move(layerBatches),
                              std::move(connectionBuffers),
                              std::move(roomNameBatchesMap)
                          );
                          // SharedMapBatchFinisher is now std::shared_ptr<IMapBatchesFinisher> (non-const)
                          return result;
                      });
}

// The free function `finish` must take a non-const IMapBatchesFinisher&
// if its `finish` method (and thus `virt_finish`) is non-const.
void finish(IMapBatchesFinisher &finisher, // Changed to non-const IMapBatchesFinisher&
            std::optional<MapBatches> &opt_batches,
            OpenGL &gl,
            GLFont &font,
            const std::optional<RoomIdSet>& roomsJustUpdated)
{
    // If it's a partial update and there are no existing batches, create them.
    // For a full update (roomsJustUpdated is nullopt), we clear existing batches.
    if (!roomsJustUpdated) {
       opt_batches.reset();
    }
    // Ensure MapBatches object exists, especially for the first update (full or partial).
    if (!opt_batches) {
        opt_batches.emplace();
    }
    MapBatches &batches = deref(opt_batches);

    finisher.finish(batches, gl, font, roomsJustUpdated); // Call the non-const finish method
}
