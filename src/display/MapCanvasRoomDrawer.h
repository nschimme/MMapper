#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Nils Schimmelmann <nschimme@gmail.com> (Jahara)

#include "../global/Array.h"
#include "../global/Timer.h"
#include "../map/ExitDirection.h"
#include "../map/coordinate.h"
#include "../map/infomark.h"
#include "../map/room.h"
#include "../map/roomid.h"
#include "../opengl/Font.h"
#include "Connections.h"
#include "IMapBatchesFinisher.h"
#include "Infomarks.h"
#include "MapBatches.h" // Already updated for RoomArea keys
#include "MapCanvasData.h"
#include "RoadIndex.h"

#include <map>
#include <optional>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <QColor>
#include <QtCore>
#include <algorithm>
#include <string_view>
#include <cassert>

class OpenGL;
class QOpenGLTexture;
struct MapCanvasTextures;

struct NODISCARD VisitRoomOptions final
{
    SharedCanvasNamedColorOptions canvasColors;
    SharedNamedColorOptions colorSettings;
    bool drawNotMappedExits = false;
};

struct NODISCARD RoomTex
{
    RoomHandle room;
    MMTextureId tex = INVALID_MM_TEXTURE_ID;
    explicit RoomTex(RoomHandle moved_room, const MMTextureId input_texid);
    NODISCARD MMTextureId priority() const { return tex; }
    NODISCARD MMTextureId textureId() const { return tex; }
    NODISCARD friend bool operator<(const RoomTex &lhs, const RoomTex &rhs)
    {
        return lhs.priority() < rhs.priority();
    }
};

struct NODISCARD ColoredRoomTex : public RoomTex
{
    Color color;
    ColoredRoomTex(const RoomHandle &room, const MMTextureId tex) = delete;
    explicit ColoredRoomTex(RoomHandle moved_room,
                            const MMTextureId input_texid,
                            const Color &input_color);
};

struct NODISCARD RoomTexVector final : public std::vector<RoomTex>
{
    void sortByTexture();
    NODISCARD bool isSorted() const;
};

struct NODISCARD ColoredRoomTexVector final : public std::vector<ColoredRoomTex>
{
    void sortByTexture();
    NODISCARD bool isSorted() const;
};

template<typename T, typename Callback>
void foreach_texture(const T &textures, Callback &&callback); // Declaration only

inline UniqueMeshVector createSortedTexturedMeshes(const std::string_view /*what*/,
                                                  OpenGL &gl,
                                                  const RoomTexVector &textures); // Declaration only

inline UniqueMeshVector createSortedColoredTexturedMeshes(
    const std::string_view /*what*/, OpenGL &gl, const ColoredRoomTexVector &textures); // Declaration only

using PlainQuadBatch = std::vector<glm::vec3>;

struct NODISCARD LayerBatchData final
{
    RoomArea m_generating_area;
    RoomTexVector roomTerrains;
    RoomTexVector roomTrails;
    RoomTexVector roomOverlays;
    ColoredRoomTexVector doors;
    ColoredRoomTexVector solidWallLines;
    ColoredRoomTexVector dottedWallLines;
    ColoredRoomTexVector roomUpDownExits;
    ColoredRoomTexVector streamIns;
    ColoredRoomTexVector streamOuts;
    RoomTintArray<PlainQuadBatch> roomTints;
    PlainQuadBatch roomLayerBoostQuads;

    explicit LayerBatchData(RoomArea generating_area = RoomArea{})
        : m_generating_area(std::move(generating_area)) {}
    ~LayerBatchData() = default;
    DEFAULT_MOVES(LayerBatchData);
    DELETE_COPIES(LayerBatchData);

    void sort();
    NODISCARD LayerMeshes getMeshes(OpenGL &gl) const;
};

struct NODISCARD InternalData final : public IMapBatchesFinisher
{
public:
    std::map<int, std::map<RoomArea, LayerBatchData>> batchedMeshes;
    std::map<int, std::map<RoomArea, ConnectionDrawerBuffers>> connectionDrawerBuffers;
    std::map<int, std::map<RoomArea, RoomNameBatch>> roomNameBatches;
private:
    void virt_finish(MapBatches &output, OpenGL &gl, GLFont &font) const final;
};

using RoomVector = std::vector<RoomHandle>;
using LayerToRooms = std::map<int, RoomVector>; // Still used by generateMapDataFinisher if it's kept/adapted

struct NODISCARD RemeshCookie final
{
private:
    std::optional<FutureSharedMapBatchFinisher> m_opt_future;
    bool m_ignored = false;
private:
    void reset() { *this = RemeshCookie{}; }
public:
    void setIgnored() { m_ignored = true; }
public:
    NODISCARD bool isPending() const { return m_opt_future.has_value(); }
    NODISCARD bool isReady() const;
private:
    static void reportException();
public:
    NODISCARD SharedMapBatchFinisher get();
public:
    void set(FutureSharedMapBatchFinisher future);
};

struct NODISCARD Batches final // This is MapCanvas::Batches
{
    RemeshCookie remeshCookie;
    std::optional<MapBatches> mapBatches; // This is the MapBatches struct from MapBatches.h
    std::optional<BatchedInfomarksMeshes> infomarksMeshes;
    struct NODISCARD FlashState final { /*...*/ };
    FlashState pendingUpdateFlashState;
    Batches() = default;
    ~Batches() = default;
    DEFAULT_MOVES_DELETE_COPIES(Batches);
    void resetExistingMeshesButKeepPendingRemesh();
    void ignorePendingRemesh();
    void resetExistingMeshesAndIgnorePendingRemesh();
};

// generateMapDataFinisher is still active as it's used for full remesh.
// NODISCARD FutureSharedMapBatchFinisher
// generateMapDataFinisher(const mctp::MapCanvasTexturesProxy &textures, const Map &map);
// The above was incorrectly commented out in a previous step's manual reconstruction.
// It should be:
NODISCARD FutureSharedMapBatchFinisher
generateMapDataFinisher(const mctp::MapCanvasTexturesProxy &textures, const Map &map);


NODISCARD FutureSharedMapBatchFinisher
generateRoomAreaDataFinisher(
    const mctp::MapCanvasTexturesProxy& textures,
    const Map& map,
    const std::vector<RoomArea>& areasToGenerate,
    const std::map<RoomArea, RoomIdSet>& areaData);

extern void finish(const IMapBatchesFinisher &finisher,
                   std::optional<MapBatches> &batches,
                   OpenGL &gl,
                   GLFont &font);

// Note: generateSpecificMapDataFinisher and generateSpecificLayerMeshes declarations are now fully deleted.
// generateAllLayerMeshes and ::generateLayerMeshes are static to MapCanvasRoomDrawer.cpp and not declared here.
