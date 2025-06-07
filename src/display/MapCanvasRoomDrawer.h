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
#include <string_view> // For std::string_view
#include <cassert> // For assert

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

// --- Static utility functions moved from MapCanvasRoomDrawer.cpp ---

// Template functions are implicitly inline
template<typename T, typename Callback>
void foreach_texture(const T &textures, Callback &&callback)
{
    assert(textures.isSorted()); // Make sure RoomTexVector/ColoredRoomTexVector's isSorted() is available if this is used with them.
                                 // For std::vector, this would rely on prior sort. Given the context, it's for RoomTexVector like types.

    const auto size = textures.size();
    for (size_t beg = 0, next = size; beg < size; beg = next) {
        // Assuming T is a collection of items that have textureId() method
        // and that items are sorted by this textureId().
        // Example for RoomTex:
        // const RoomTex &rtex = textures[beg];
        // const auto textureId = rtex.textureId();
        // This generic version might need adjustment if T is not RoomTexVector or ColoredRoomTexVector
        // For now, assuming it's used with types like RoomTexVector which have textureId() on elements.
        // A more generic approach would require a projection function for textureId.
        // However, copying the exact implementation from .cpp:
        const auto& first_element_in_batch = textures[beg]; // Requires T to be indexable and have textureId()
        const auto textureId = first_element_in_batch.textureId();


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

inline UniqueMeshVector createSortedTexturedMeshes(const std::string_view /*what*/,
                                                  OpenGL &gl,
                                                  const RoomTexVector &textures)
{
    if (textures.empty()) {
        return UniqueMeshVector{};
    }

    const size_t numUniqueTextures = [&textures]() -> size_t {
        size_t texCount = 0;
        ::foreach_texture(textures, // Assuming global namespace or correct scope resolution for foreach_texture
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

        for (size_t i = beg; i < end; ++i) {
            const auto &pos = textures[i].room.getPosition();
            const auto v0 = pos.to_vec3();
            verts.emplace_back(glm::vec2(0, 0), v0 + glm::vec3(0, 0, 0)); // A
            verts.emplace_back(glm::vec2(1, 0), v0 + glm::vec3(1, 0, 0)); // B
            verts.emplace_back(glm::vec2(1, 1), v0 + glm::vec3(1, 1, 0)); // C
            verts.emplace_back(glm::vec2(0, 1), v0 + glm::vec3(0, 1, 0)); // D
        }

        result_meshes.emplace_back(gl.createTexturedQuadBatch(verts, rtex.tex));
    };

    ::foreach_texture(textures, lambda); // Assuming global namespace or correct scope
    assert(result_meshes.size() == numUniqueTextures);
    return UniqueMeshVector{std::move(result_meshes)};
}

inline UniqueMeshVector createSortedColoredTexturedMeshes(
    const std::string_view /*what*/, OpenGL &gl, const ColoredRoomTexVector &textures)
{
    if (textures.empty()) {
        return UniqueMeshVector{};
    }

    const size_t numUniqueTextures = [&textures]() -> size_t {
        size_t texCount = 0;
        ::foreach_texture(textures, // Assuming global namespace or correct scope
                          [&texCount](size_t /* beg */, size_t /*end*/) -> void { ++texCount; });
        return texCount;
    }();

    std::vector<UniqueMesh> result_meshes;
    result_meshes.reserve(numUniqueTextures);

    const auto lambda = [&gl, &result_meshes, &textures](const size_t beg,
                                                         const size_t end) -> void {
        const RoomTex &rtex = textures[beg]; // Note: textures is ColoredRoomTexVector, rtex should be ColoredRoomTex
        const size_t count = end - beg;

        std::vector<ColoredTexVert> verts;
        verts.reserve(count * VERTS_PER_QUAD); /* quads */

        for (size_t i = beg; i < end; ++i) {
            const ColoredRoomTex &thisVert = textures[i];
            const auto &pos = thisVert.room.getPosition();
            const auto v0 = pos.to_vec3();
            const auto color = thisVert.color;

            verts.emplace_back(color, glm::vec2(0, 0), v0 + glm::vec3(0, 0, 0)); // A
            verts.emplace_back(color, glm::vec2(1, 0), v0 + glm::vec3(1, 0, 0)); // B
            verts.emplace_back(color, glm::vec2(1, 1), v0 + glm::vec3(1, 1, 0)); // C
            verts.emplace_back(color, glm::vec2(0, 1), v0 + glm::vec3(0, 1, 0)); // D
        }

        result_meshes.emplace_back(gl.createColoredTexturedQuadBatch(verts, rtex.textureId())); // Use textureId()
    };

    ::foreach_texture(textures, lambda); // Assuming global namespace or correct scope
    assert(result_meshes.size() == numUniqueTextures);
    return UniqueMeshVector{std::move(result_meshes)};
}

// --- End of static utility functions ---

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
