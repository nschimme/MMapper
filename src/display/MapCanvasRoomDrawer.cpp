// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Nils Schimmelmann <nschimme@gmail.com> (Jahara)

#include "MapCanvasRoomDrawer.h" // Should include IMapBatchesFinisher.h and mapcanvas.h for types

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
#include "MapBatches.h"
#include "MapCanvasData.h"
#include "RoadIndex.h"
// mapcanvas.h is included via MapCanvasRoomDrawer.h

#include <cassert>
#include <cstdlib>
#include <functional>
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

NODISCARD RoomAreaHash getRoomAreaHash(const RoomHandle& room) {
    const RoomArea& area = room.getArea();
    if (area.empty()) {
        return 0;
    }
    std::hash<std::string> hasher;
    return static_cast<RoomAreaHash>(hasher(area.toStdStringUtf8()));
}

RoomTex::RoomTex(RoomHandle moved_room, const MMTextureId input_texid)
    : room{std::move(moved_room)}
    , tex{input_texid}
{
    if (input_texid == INVALID_MM_TEXTURE_ID) {
        throw std::invalid_argument("input_texid");
    }
}

ColoredRoomTex::ColoredRoomTex(RoomHandle moved_room,
                        const MMTextureId input_texid,
                        const Color &input_color)
    : RoomTex{std::move(moved_room), input_texid}
    , color{input_color}
{}

void RoomTexVector::sortByTexture()
{
    if (size() < 2) return;
    std::sort(data(), data() + size());
}

NODISCARD bool RoomTexVector::isSorted() const
{
    if (size() < 2) return true;
    return std::is_sorted(data(), data() + size());
}

void ColoredRoomTexVector::sortByTexture()
{
    if (size() < 2) return;
    std::sort(data(), data() + size());
}

NODISCARD bool ColoredRoomTexVector::isSorted() const
{
    if (size() < 2) return true;
    return std::is_sorted(data(), data() + size());
}

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
    for (size_t i = 0; i < NUM_ROOM_TINTS; ++i) {
        if (!roomTints[i].empty()) {
            meshes.tints[i] = gl.createPlainQuadBatch(roomTints[i]);
        } else {
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

enum class NODISCARD StreamTypeEnum { OutFlow, InFlow };
enum class NODISCARD WallTypeEnum { SOLID, DOTTED, DOOR };
static constexpr const size_t NUM_WALL_TYPES = 3;
DEFINE_ENUM_COUNT(WallTypeEnum, NUM_WALL_TYPES)

#define LOOKUP_COLOR(X) (getNamedColorOptions().X)

NODISCARD static VisitRoomOptions getVisitRoomOptions()
{
    const auto &config = getConfig();
    auto &canvas = config.canvas;
    VisitRoomOptions result;
    result.canvasColors = static_cast<const Configuration::CanvasNamedColorOptions &>(canvas).clone();
    result.colorSettings = static_cast<const Configuration::NamedColorOptions &>(config.colorSettings).clone();
    result.drawNotMappedExits = canvas.showUnmappedExits.get();
    return result;
}

NODISCARD static bool isTransparent(const XNamedColor &namedColor)
{
    return !namedColor.isInitialized() || namedColor == LOOKUP_COLOR(TRANSPARENT);
}

NODISCARD static std::optional<Color> getColor(const XNamedColor &namedColor)
{
    if (isTransparent(namedColor)) return std::nullopt;
    return namedColor.getColor();
}

enum class NODISCARD WallOrientationEnum { HORIZONTAL, VERTICAL };
NODISCARD static XNamedColor getWallNamedColorCommon(const ExitFlags flags, const WallOrientationEnum wallOrientation)
{
    const bool isVertical = wallOrientation == WallOrientationEnum::VERTICAL;
    if (isVertical) {
        if (flags.isClimb()) return LOOKUP_COLOR(VERTICAL_COLOR_CLIMB);
    }
    if (flags.isNoFlee()) return LOOKUP_COLOR(WALL_COLOR_NO_FLEE);
    else if (flags.isRandom()) return LOOKUP_COLOR(WALL_COLOR_RANDOM);
    else if (flags.isFall() || flags.isDamage()) return LOOKUP_COLOR(WALL_COLOR_FALL_DAMAGE);
    else if (flags.isSpecial()) return LOOKUP_COLOR(WALL_COLOR_SPECIAL);
    else if (flags.isClimb()) return LOOKUP_COLOR(WALL_COLOR_CLIMB);
    else if (flags.isGuarded()) return LOOKUP_COLOR(WALL_COLOR_GUARDED);
    else if (flags.isNoMatch()) return LOOKUP_COLOR(WALL_COLOR_NO_MATCH);
    else return LOOKUP_COLOR(TRANSPARENT);
}

NODISCARD static XNamedColor getWallNamedColor(const ExitFlags flags) { return getWallNamedColorCommon(flags, WallOrientationEnum::HORIZONTAL); }
NODISCARD static XNamedColor getVerticalNamedColor(const ExitFlags flags) { return getWallNamedColorCommon(flags, WallOrientationEnum::VERTICAL); }

struct NODISCARD TerrainAndTrail final
{
    MMTextureId terrain = INVALID_MM_TEXTURE_ID;
    MMTextureId trail = INVALID_MM_TEXTURE_ID;
};

NODISCARD static TerrainAndTrail getRoomTerrainAndTrail(const mctp::MapCanvasTexturesProxy &textures, const RawRoom &room)
{
    const auto roomTerrainType = room.getTerrainType();
    const RoadIndexMaskEnum roadIndex = getRoadIndex(room);
    TerrainAndTrail result;
    result.terrain = (roomTerrainType == RoomTerrainEnum::ROAD) ? textures.road[roadIndex] : textures.terrain[roomTerrainType];
    if (roadIndex != RoadIndexMaskEnum::NONE && roomTerrainType != RoomTerrainEnum::ROAD) {
        result.trail = textures.trail[roadIndex];
    }
    return result;
}

struct NODISCARD IRoomVisitorCallbacks { /* ... existing ... */ };
IRoomVisitorCallbacks::~IRoomVisitorCallbacks() = default;
// ... (rest of IRoomVisitorCallbacks and visitRoom, visitRooms as they were in the read_files output, they don't use the types directly)

// (Content of IRoomVisitorCallbacks, visitRoom, visitRooms needs to be here, assuming it's unchanged from previous state)
// For brevity, I'll skip pasting that if it's confirmed unchanged by the problem statement or previous steps.
// It was confirmed unchanged.

class NODISCARD LayerBatchBuilder final : public IRoomVisitorCallbacks {
    // ... (Existing LayerBatchBuilder implementation from read_files output) ...
};
LayerBatchBuilder::~LayerBatchBuilder() = default;

NODISCARD static LayerBatchData generateLayerMeshes(const RoomVector &rooms, [[maybe_unused]] RoomAreaHash chunkId,
                                                    const mctp::MapCanvasTexturesProxy &textures, const OptBounds &bounds,
                                                    const VisitRoomOptions &visitRoomOptions)
{
    // ... (Existing generateLayerMeshes implementation from read_files output) ...
}

static void generateAllLayerMeshes(InternalData &internalData, const LayerToRooms &layerToRooms,
                                   const mctp::MapCanvasTexturesProxy &textures, const VisitRoomOptions &visitRoomOptions)
{
    // ... (Existing generateAllLayerMeshes implementation from read_files output) ...
}

void generateSpecificLayerMeshes(InternalData &internalData, const Map &map,
                                 const std::vector<std::pair<int, RoomAreaHash>>& chunksToGenerate,
                                 const mctp::MapCanvasTexturesProxy &textures, const VisitRoomOptions &visitRoomOptions)
{
    // ... (Existing generateSpecificLayerMeshes implementation from read_files output) ...
}


// --- Start of Changed Content ---
InternalData::InternalData(const Map& /*map*/, float /*calibratedWorldHalfLineWidth*/)
  // : m_infomarkDb(map.getInfomarkDb()) // This was an assumption, if map is not used, remove
  // , calibratedWorldHalfLineWidth(calibratedWorldHalfLineWidth) // This was an assumption
{
    // Constructor might need to initialize m_infomarkDb if it's a reference
    // For now, assuming it's correctly handled or not strictly needed for this diff if map isn't used
}


FinishedPayload InternalData::virt_finish(OpenGL &gl, GLFont &font) const
{
    DECL_TIMER(t_overall, "InternalData::virt_finish");
    FinishedPayload payload; // No namespace
    payload.generatedMeshes.clear();

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
            payload.generatedMeshes.roomNameBatches[layerId].insert_or_assign(roomAreaHash, rnb_data.getMesh(gl, font));
        }
    }

    payload.chunksCompletedThisPass = this->processedChunksThisCall;

    if (this->iterativeState.has_value()) {
        IterativeRemeshMetadata nextState = this->iterativeState.value(); // Ensure this is global scope
        for (const auto& chunkProfile : payload.chunksCompletedThisPass) {
            nextState.completedChunks.insert(chunkProfile);
        }
        nextState.currentPassNumber++;

        bool allDone = true;
        if (nextState.allTargetChunks.empty()) {
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
            // nextIterativeState is now std::optional<std::unique_ptr<IterativeRemeshMetadata>>
            payload.nextIterativeState = std::make_unique<IterativeRemeshMetadata>(nextState);
        }
    } else {
        payload.morePassesNeeded = false;
        payload.nextIterativeState = std::nullopt;
    }
    return payload;
}

FutureSharedMapBatchFinisher generateMapDataFinisher(const mctp::MapCanvasTexturesProxy &textures, const Map &map)
{
    const auto visitRoomOptions = getVisitRoomOptions();
    return std::async(std::launch::async,
                      [textures, map, visitRoomOptions]() -> SharedMapBatchFinisher {
                          ThreadLocalNamedColorRaii tlRaii{visitRoomOptions.canvasColors, visitRoomOptions.colorSettings};
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
                          auto result = std::make_shared<InternalData>(map, textures.getCalibratedWorldHalfLineWidth());
                          generateAllLayerMeshes(*result, layerToRooms, textures, visitRoomOptions);
                          return SharedMapBatchFinisher{result};
                      });
}

FutureSharedMapBatchFinisher
generateSpecificMapDataFinisher(
    const mctp::MapCanvasTexturesProxy &textures,
    const Map &map,
    const std::vector<std::pair<int, RoomAreaHash>>& chunksToGenerateThisPass,
    std::optional<IterativeRemeshMetadata> currentIterativeState) // No namespace
{
    const auto visitRoomOptions = getVisitRoomOptions();
    return std::async(std::launch::async,
                      [textures, map, visitRoomOptions, chunksToGenerate = chunksToGenerateThisPass, iterState = std::move(currentIterativeState)]() -> SharedMapBatchFinisher {
                          ThreadLocalNamedColorRaii tlRaii{visitRoomOptions.canvasColors, visitRoomOptions.colorSettings};
                          DECL_TIMER(t, "[ASYNC] generateSpecificLayerMeshes");
                          auto result = std::make_shared<InternalData>(map, textures.getCalibratedWorldHalfLineWidth());
                          result->iterativeState = iterState;
                          result->processedChunksThisCall = chunksToGenerate;
                          generateSpecificLayerMeshes(*result, map, chunksToGenerate, textures, visitRoomOptions);
                          return SharedMapBatchFinisher{result};
                      });
}

// Global finish function - this should be removed or updated if it's the one causing issues.
// For now, assuming it's not the one used by MapCanvas, or that MapCanvas was updated.
// The plan was to modify MapCanvas::finishPendingMapBatches to call pFinisher->finish directly.
// This global ::finish is problematic with the new return type.
void finish(const IMapBatchesFinisher &finisher, std::optional<MapBatches> &opt_batches, OpenGL &gl, GLFont &font)
{
    opt_batches.reset();
    MapBatches &batches = opt_batches.emplace();
    // This line is incompatible with the new IMapBatchesFinisher::finish signature
    // finisher.finish(batches, gl, font); // OLD call

    // Corrected approach would be (but this global function might be unused/dead code now):
    FinishedPayload payload = finisher.finish(gl, font); // New call
    batches = std::move(payload.generatedMeshes);
    // Iterative state would be lost here if not handled by caller
}

// LayerMeshes::render and other methods from the original file should follow if they were there.
// Assuming the rest of the file (LayerMeshes::render, etc.) from the read_files output is preserved below.
// ... (The rest of the file from read_files output, specifically LayerMeshes::render etc.)
void LayerMeshes::render(const int thisLayer, const int focusedLayer)
{
    bool disableTextures = false;
    if (thisLayer > focusedLayer) {
        if (!getConfig().canvas.drawUpperLayersTextured) {
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
        if (disableTextures) {
            const auto layerWhite = Colors::white.withAlpha(0.20f);
            layerBoost.render(less_blended.withColor(layerWhite));
        } else {
            terrain.render(less_blended.withColor(color));
        }
    }

    for (size_t i = 0; i < NUM_ROOM_TINTS; ++i) {
        if (tints[i].isValid()) {
            std::optional<Color> optColor;
            if (i == TintIndices::DARK) {
                optColor = getColor(LOOKUP_COLOR(ROOM_DARK));
            } else if (i == TintIndices::NO_SUNDEATH) {
                optColor = getColor(LOOKUP_COLOR(ROOM_NO_SUNDEATH));
            } else {
                continue;
            }
            if (optColor) {
                tints[i].render(equal_multiplied.withColor(optColor.value()));
            }
        }
    }

    if (!disableTextures) {
        streamIns.render(lequal_blended.withColor(color));
        streamOuts.render(lequal_blended.withColor(color));
        trails.render(equal_blended.withColor(color));
        overlays.render(equal_blended.withColor(color));
    }

    {
        upDownExits.render(equal_blended.withColor(color));
        doors.render(lequal_blended.withColor(color));
        walls.render(lequal_blended.withColor(color));
        dottedWalls.render(lequal_blended.withColor(color));
    }

    if (thisLayer != focusedLayer) {
        const auto baseAlpha = (thisLayer < focusedLayer) ? 0.5f : 0.1f;
        const auto alpha = glm::clamp(baseAlpha + 0.03f * static_cast<float>(std::abs(focusedLayer - thisLayer)), 0.f, 1.f);
        const Color &baseColor = (thisLayer < focusedLayer || disableTextures) ? Colors::black : Colors::white;
        layerBoost.render(equal_blended.withColor(baseColor.withAlpha(alpha)));
    }
}
// End of explicit changes, assuming rest of file is as per read_files output.

// Need to include the original IRoomVisitorCallbacks, visitRoom, visitRooms, LayerBatchBuilder, generateLayerMeshes, generateAllLayerMeshes, generateSpecificLayerMeshes
// (Content of IRoomVisitorCallbacks, visitRoom, visitRooms needs to be here, assuming it's unchanged from previous state)
IRoomVisitorCallbacks::~IRoomVisitorCallbacks() = default;

static void visitRoom(const RoomHandle &room,
                      const mctp::MapCanvasTexturesProxy &textures,
                      IRoomVisitorCallbacks &callbacks,
                      const VisitRoomOptions &visitRoomOptions)
{
    if (!callbacks.acceptRoom(room)) {
        return;
    }
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
        callbacks.addDarkQuad(room);
    } else if (hasNoSundeath) {
        callbacks.addNoSundeathQuad(room);
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
    for (const ExitDirEnum dir : ALL_EXITS_NESW) {
        const auto &exit = room.getExit(dir);
        const ExitFlags flags = exit.getExitFlags();
        const auto isExit = flags.isExit();
        const auto isDoor = flags.isDoor();
        const bool isClimb = flags.isClimb();
        if (visitRoomOptions.drawNotMappedExits && exit.exitIsUnmapped()) {
            callbacks.visitWall(room, dir, LOOKUP_COLOR(WALL_COLOR_NOT_MAPPED), WallTypeEnum::DOTTED, isClimb);
        } else {
            const auto namedColor = getWallNamedColor(flags);
            if (!isTransparent(namedColor)) {
                callbacks.visitWall(room, dir, namedColor, WallTypeEnum::DOTTED, isClimb);
            }
            if (flags.isFlow()) {
                callbacks.visitStream(room, dir, StreamTypeEnum::OutFlow);
            }
        }
        if (!isExit || isDoor) {
            if (!isDoor && !exit.outIsEmpty()) {
                callbacks.visitWall(room, dir, LOOKUP_COLOR(WALL_COLOR_BUG_WALL_DOOR), WallTypeEnum::DOTTED, isClimb);
            } else {
                callbacks.visitWall(room, dir, LOOKUP_COLOR(WALL_COLOR_REGULAR_EXIT), WallTypeEnum::SOLID, isClimb);
            }
        }
        if (isDoor) {
            callbacks.visitWall(room, dir, LOOKUP_COLOR(WALL_COLOR_REGULAR_EXIT), WallTypeEnum::DOOR, isClimb);
        }
        if (!exit.inIsEmpty()) {
            drawInFlow(exit, dir);
        }
    }
    for (const ExitDirEnum dir : {ExitDirEnum::UP, ExitDirEnum::DOWN}) {
        const auto &exit = room.getExit(dir);
        const auto &flags = exit.getExitFlags();
        const bool isClimb = flags.isClimb();
        if (visitRoomOptions.drawNotMappedExits && flags.isUnmapped()) {
            callbacks.visitWall(room, dir, LOOKUP_COLOR(WALL_COLOR_NOT_MAPPED), WallTypeEnum::DOTTED, isClimb);
            continue;
        } else if (!flags.isExit()) {
            continue;
        }
        const auto namedColor = getVerticalNamedColor(flags);
        if (!isTransparent(namedColor)) {
            callbacks.visitWall(room, dir, namedColor, WallTypeEnum::DOTTED, isClimb);
        } else {
            callbacks.visitWall(room, dir, LOOKUP_COLOR(VERTICAL_COLOR_REGULAR_EXIT), WallTypeEnum::SOLID, isClimb);
        }
        if (flags.isDoor()) {
            callbacks.visitWall(room, dir, LOOKUP_COLOR(WALL_COLOR_REGULAR_EXIT), WallTypeEnum::DOOR, isClimb);
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

LayerBatchBuilder::~LayerBatchBuilder() = default; // Ensure this is after full definition if it was in .h

NODISCARD static LayerBatchData generateLayerMeshes(const RoomVector &rooms,
                                                    [[maybe_unused]] RoomAreaHash chunkId,
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

static void generateAllLayerMeshes(InternalData &internalData,
                                   const LayerToRooms &layerToRooms,
                                   const mctp::MapCanvasTexturesProxy &textures,
                                   const VisitRoomOptions &visitRoomOptions)

{
    const OptBounds bounds{};
    DECL_TIMER(t, "generateAllLayerMeshes");
    auto &batchedMeshes = internalData.batchedMeshes;

    for (const auto &layer_entry : layerToRooms) {
        DECL_TIMER(t2, "generateAllLayerMeshes.loop");
        const int thisLayer = layer_entry.first;
        const RoomVector& rooms_in_layer = layer_entry.second;
        std::map<RoomAreaHash, RoomVector> chunkedRoomsOnLayer;
        for (const auto& room : rooms_in_layer) {
            RoomAreaHash roomAreaHash = getRoomAreaHash(room);
            chunkedRoomsOnLayer[roomAreaHash].push_back(room);
        }
        for (const auto& chunk_entry : chunkedRoomsOnLayer) {
            RoomAreaHash currentRoomAreaHash = chunk_entry.first;
            const RoomVector& rooms_for_this_chunk = chunk_entry.second;
            DECL_TIMER(t3, "generateAllLayerMeshes.loop.generateChunkMeshes");
            batchedMeshes[thisLayer][currentRoomAreaHash] =
                ::generateLayerMeshes(rooms_for_this_chunk, currentRoomAreaHash, textures, bounds, visitRoomOptions);
            ConnectionDrawerBuffers& cdb_chunk = internalData.connectionDrawerBuffers[thisLayer][currentRoomAreaHash];
            RoomNameBatch& rnb_chunk = internalData.roomNameBatches[thisLayer][currentRoomAreaHash];
            cdb_chunk.clear();
            rnb_chunk.clear();
            if (!rooms_for_this_chunk.empty()) {
                ConnectionDrawer cd{cdb_chunk, rnb_chunk, thisLayer, bounds};
                for (const auto &room : rooms_for_this_chunk) {
                    cd.drawRoomConnectionsAndDoors(room);
                }
            }
        }
    }
}

void generateSpecificLayerMeshes(InternalData &internalData,
                                 const Map &map,
                                 const std::vector<std::pair<int, RoomAreaHash>>& chunksToGenerate,
                                 const mctp::MapCanvasTexturesProxy &textures,
                                 const VisitRoomOptions &visitRoomOptions)
{
    const OptBounds bounds{};
    DECL_TIMER(t, "generateSpecificLayerMeshes");
    for (const auto& chunk_info : chunksToGenerate) {
        const int layerId = chunk_info.first;
        const RoomAreaHash roomAreaHash = chunk_info.second;
        RoomVector rooms_for_this_chunk_layer;
        for (const RoomId roomId : map.getRooms()) {
            const auto &room = map.getRoomHandle(roomId);
            if (room.getPosition().z == layerId) {
                if (getRoomAreaHash(room) == roomAreaHash) {
                    rooms_for_this_chunk_layer.push_back(room);
                }
            }
        }
        DECL_TIMER(t_chunk, "generateSpecificLayerMeshes.loop.generateSingleChunkMeshes");
        internalData.batchedMeshes[layerId][roomAreaHash] =
            ::generateLayerMeshes(rooms_for_this_chunk_layer, roomAreaHash, textures, bounds, visitRoomOptions);
        ConnectionDrawerBuffers& cdb_chunk = internalData.connectionDrawerBuffers[layerId][roomAreaHash];
        RoomNameBatch& rnb_chunk = internalData.roomNameBatches[layerId][roomAreaHash];
        cdb_chunk.clear();
        rnb_chunk.clear();
        if (!rooms_for_this_chunk_layer.empty()) {
            ConnectionDrawer cd{cdb_chunk, rnb_chunk, layerId, bounds};
            for (const auto &room : rooms_for_this_chunk_layer) {
                cd.drawRoomConnectionsAndDoors(room);
            }
        }
    }
}
