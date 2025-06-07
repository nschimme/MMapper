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
#include "MapBatches.h"
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
#include <algorithm> // For std::sort, std::is_sorted

// Forward declarations
class OpenGL;
class QOpenGLTexture;
struct MapCanvasTextures; // Defined in Textures.h, but forward declaration is fine if only pointers/references used.
                          // However, mctp::MapCanvasTexturesProxy uses it, which is included by MapCanvasData.h->Textures.h

// Moved from MapCanvasRoomDrawer.cpp
// Depends on SharedCanvasNamedColorOptions and SharedNamedColorOptions from MapCanvasData.h -> configuration.h
struct NODISCARD VisitRoomOptions final
{
    SharedCanvasNamedColorOptions canvasColors;
    SharedNamedColorOptions colorSettings;
    bool drawNotMappedExits = false;
};

// --- Structs moved from MapCanvasRoomDrawer.cpp ---

struct NODISCARD RoomTex
{
    RoomHandle room;
    MMTextureId tex = INVALID_MM_TEXTURE_ID;

    explicit RoomTex(RoomHandle moved_room, const MMTextureId input_texid);
    // Implementation in .cpp

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
    ColoredRoomTex(const RoomHandle &room, const MMTextureId tex) = delete; // Prevent accidental slicing

    explicit ColoredRoomTex(RoomHandle moved_room,
                            const MMTextureId input_texid,
                            const Color &input_color);
    // Implementation in .cpp
};

struct NODISCARD RoomTexVector final : public std::vector<RoomTex>
{
    void sortByTexture(); // Implementation in .cpp
    NODISCARD bool isSorted() const; // Implementation in .cpp
};

struct NODISCARD ColoredRoomTexVector final : public std::vector<ColoredRoomTex>
{
    void sortByTexture(); // Implementation in .cpp
    NODISCARD bool isSorted() const; // Implementation in .cpp
};

using PlainQuadBatch = std::vector<glm::vec3>;

struct NODISCARD LayerBatchData final
{
    RoomTexVector roomTerrains;
    RoomTexVector roomTrails;
    RoomTexVector roomOverlays;
    ColoredRoomTexVector doors;
    ColoredRoomTexVector solidWallLines;
    ColoredRoomTexVector dottedWallLines;
    ColoredRoomTexVector roomUpDownExits;
    ColoredRoomTexVector streamIns;
    ColoredRoomTexVector streamOuts;
    RoomTintArray<PlainQuadBatch> roomTints; // Provided by MapBatches.h -> mmapper2room.h and EnumIndexedArray.h
    PlainQuadBatch roomLayerBoostQuads;

    explicit LayerBatchData() = default;
    ~LayerBatchData() = default;
    DEFAULT_MOVES(LayerBatchData); // Assumes RuleOf5.h provides this, or define manually
    DELETE_COPIES(LayerBatchData); // Assumes RuleOf5.h provides this, or define manually

    void sort(); // Implementation in .cpp
    NODISCARD LayerMeshes getMeshes(OpenGL &gl) const; // Implementation in .cpp
};

// --- End of structs moved from MapCanvasRoomDrawer.cpp ---


// Moved from MapCanvasRoomDrawer.cpp
// Depends on IMapBatchesFinisher (IMapBatchesFinisher.h)
// Depends on ChunkId, LayerBatchData, RoomNameBatch (MapBatches.h)
// Depends on ConnectionDrawerBuffers (Connections.h)
// Depends on OpenGL, GLFont for virt_finish signature (OpenGL.h, Font.h)
struct NODISCARD InternalData final : public IMapBatchesFinisher
{
public:
    // Key: layerId, Value: map of ChunkId to its LayerBatchData
    std::unordered_map<int, std::map<ChunkId, LayerBatchData>> batchedMeshes;

    // Key: layerId, Value: map of ChunkId to its ConnectionDrawerBuffers
    std::map<int, std::map<ChunkId, ConnectionDrawerBuffers>> connectionDrawerBuffers;

    // Key: layerId, Value: map of ChunkId to its RoomNameBatch
    std::map<int, std::map<ChunkId, RoomNameBatch>> roomNameBatches;

private:
    // Implementation remains in MapCanvasRoomDrawer.cpp
    void virt_finish(MapBatches &output, OpenGL &gl, GLFont &font) const final;
};


using RoomVector = std::vector<RoomHandle>;
using LayerToRooms = std::map<int, RoomVector>;

struct NODISCARD RemeshCookie final
{
private:
    std::optional<FutureSharedMapBatchFinisher> m_opt_future;
    bool m_ignored = false;

private:
    // NOTE: If you think you want to make this public, then you're really looking for setIgnored().
    void reset() { *this = RemeshCookie{}; }

public:
    // This means you've decided to load a completely new map, so you're not interested
    // in the results. It SHOULD NOT be used just because you got another mesh request.
    void setIgnored()
    {
        //
        m_ignored = true;
    }

public:
    NODISCARD bool isPending() const { return m_opt_future.has_value(); }

    // Don't call this unless isPending() is true.
    // returns true if get() will return without blocking
    NODISCARD bool isReady() const
    {
        const FutureSharedMapBatchFinisher &future = m_opt_future.value();
        return future.valid()
               && future.wait_for(std::chrono::nanoseconds(0)) != std::future_status::timeout;
    }

private:
    static void reportException()
    {
        try {
            std::rethrow_exception(std::current_exception());
        } catch (const std::exception &ex) {
            qWarning() << "Exception: " << ex.what();
        } catch (...) {
            qWarning() << "Unknown exception";
        }
    }

public:
    // Don't call this unless isPending() is true.
    // NOTE: This can throw an exception thrown by the async function!
    NODISCARD SharedMapBatchFinisher get()
    {
        DECL_TIMER(t, __FUNCTION__);
        FutureSharedMapBatchFinisher &future = m_opt_future.value();

        SharedMapBatchFinisher pFinisher;
        try {
            pFinisher = future.get();
        } catch (...) {
            reportException();
            pFinisher.reset();
        }

        if (m_ignored) {
            pFinisher.reset();
        }

        reset();
        return pFinisher;
    }

public:
    // Don't call this if isPending() is true, unless you called set_ignored().
    void set(FutureSharedMapBatchFinisher future)
    {
        if (m_opt_future && !m_ignored) {
            // If this happens, you should wait until the old one is finished first,
            // or we're going to end up with tons of in-flight future meshes.
            throw std::runtime_error("replaced existing future");
        }

        m_opt_future.emplace(std::move(future));
        m_ignored = false;
    }
};

struct NODISCARD Batches final
{
    RemeshCookie remeshCookie;
    std::optional<MapBatches> mapBatches;
    std::optional<BatchedInfomarksMeshes> infomarksMeshes;
    struct NODISCARD FlashState final
    {
    private:
        std::chrono::steady_clock::time_point lastChange = getTime();
        bool on = false;

        static std::chrono::steady_clock::time_point getTime()
        {
            return std::chrono::steady_clock::now();
        }

    public:
        NODISCARD bool tick()
        {
            const auto flash_interval = std::chrono::milliseconds(100);
            const auto now = getTime();
            if (now >= lastChange + flash_interval) {
                lastChange = now;
                on = !on;
            }
            return on;
        }
    };
    FlashState pendingUpdateFlashState;

    Batches() = default;
    ~Batches() = default;
    DEFAULT_MOVES_DELETE_COPIES(Batches);

    void resetExistingMeshesButKeepPendingRemesh()
    {
        mapBatches.reset();
        infomarksMeshes.reset();
    }

    void ignorePendingRemesh()
    {
        //
        remeshCookie.setIgnored();
    }

    void resetExistingMeshesAndIgnorePendingRemesh()
    {
        resetExistingMeshesButKeepPendingRemesh();
        ignorePendingRemesh();
    }
};

NODISCARD FutureSharedMapBatchFinisher
generateMapDataFinisher(const mctp::MapCanvasTexturesProxy &textures, const Map &map);

NODISCARD FutureSharedMapBatchFinisher
generateSpecificMapDataFinisher(const mctp::MapCanvasTexturesProxy &textures, const Map &map, const std::vector<std::pair<int, ChunkId>>& chunksToGenerate);

// Changed first parameter type from IMapBatchesFinisher::InternalData& to InternalData&
void generateSpecificLayerMeshes(InternalData &internalData,
                                 const Map &map,
                                 const std::vector<std::pair<int, ChunkId>>& chunksToGenerate,
                                 const mctp::MapCanvasTexturesProxy &textures,
                                 const VisitRoomOptions &visitRoomOptions);

extern void finish(const IMapBatchesFinisher &finisher,
                   std::optional<MapBatches> &batches,
                   OpenGL &gl,
                   GLFont &font);
