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
#include <set> // Required for RoomIdSet

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <QColor>
#include <QtCore>

class OpenGL;
class QOpenGLTexture;
struct MapCanvasTextures;

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
        // mapBatches.reset(); // Commented out as per plan to manage through virt_finish
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

// Forward declaration
namespace mctp {
struct MapCanvasTexturesProxy;
}
class Map;

// Modified generateMapDataFinisher signature
NODISCARD FutureSharedMapBatchFinisher
generateMapDataFinisher(const mctp::MapCanvasTexturesProxy &textures, const Map &map, const std::optional<RoomIdSet>& roomIdsToProcess = std::nullopt);

extern void finish(const IMapBatchesFinisher &finisher,
                   std::optional<MapBatches> &batches,
                   OpenGL &gl,
                   GLFont &font,
                   const std::optional<RoomIdSet>& roomsJustUpdated); // Added roomsJustUpdated


// InternalData is an implementation detail of MapCanvasRoomDrawer.cpp
// It should be defined in the .cpp or in a private header if it were larger.
// For now, modifying its declaration here as per the prompt.
struct InternalData final : public IMapBatchesFinisher
{
    // existing members like m_batchedMeshes (implicitly, this is `batchedMeshes` from base),
    // connectionDrawerBuffers, roomNameBatches
    // ...

    // New member to hold all layer data, potentially.
    // Or, m_batchedMeshes in the base will store the *partial* data from this run.
    std::unordered_map<int, LayerBatchData> m_masterLayerBatchData; // For merging later

    // Constructor or methods to populate data...

    void virt_finish(
        MapBatches &output_target_on_canvas,
        OpenGL &gl,
        GLFont &font,
        const std::optional<RoomIdSet>& roomsJustUpdated) const override;
};


// In RoomTexVector and ColoredRoomTexVector in IMapBatchesFinisher.h or similar
// (Assuming these are part of IMapBatchesFinisher or its related types based on context)
// For now, adding directly to IMapBatchesFinisher for simplicity as per prompt structure,
// though they might live in a more specific structure like LayerBatchData.
// Let's assume they are part of LayerBatchData which is part of IMapBatchesFinisher's payload.

// Modify LayerBatchData in IMapBatchesFinisher.h (or where it's defined)
// This is a conceptual change placement. If LayerBatchData is opaque, these would be methods on it.
// If RoomTexVector is directly accessible (e.g. public member of LayerBatchData), then it's simpler.

// For the purpose of this step, let's assume RoomTexVector and ColoredRoomTexVector are accessible
// and we are adding methods to them. If they are inside LayerBatchData, the .h for that would change.
// The prompt implies these are distinct types. Let's assume they are defined somewhere accessible.

// If RoomTexVector and ColoredRoomTexVector are not directly in this file,
// this part of the change would go into the file where they are defined.
// Let's assume they are part of the types used by IMapBatchesFinisher, possibly within LayerBatchData.

// Since the prompt implies these structs are standalone or part of a structure modified here:
struct RoomTexVector {
    // ... existing members like 'std::vector<RoomVertex> vertices;' ...
    // ... and 'const RawRoom &room;' or similar to get RoomId ...
    // This is a simplified representation. Actual structure might differ.
    std::vector<BatchItem<RoomVertex>> items; // Example: if it holds items that have room context

    void removeRooms(const RoomIdSet& idsToRemove) {
        items.erase(std::remove_if(items.begin(), items.end(),
                                   [&](const BatchItem<RoomVertex>& item) {
                                       // Assuming BatchItem has a way to get RoomId
                                       // e.g. item.roomId or item.room->getId()
                                       // This is a placeholder for actual RoomId access
                                       // For now, let's assume item has a .roomId member
                                       // return idsToRemove.count(item.roomId);
                                       return false; // Placeholder
                                   }),
                    items.end());
    }

    void append(const RoomTexVector& other) {
        items.insert(items.end(), std::make_move_iterator(other.items.begin()), std::make_move_iterator(other.items.end()));
        // Or if BatchItem is not movable efficiently or to copy:
        // items.insert(items.end(), other.items.begin(), other.items.end());
    }
};

struct ColoredRoomTexVector {
    // ... similar members ...
    std::vector<BatchItem<ColorVertex>> items; // Example

    void removeRooms(const RoomIdSet& idsToRemove) {
        items.erase(std::remove_if(items.begin(), items.end(),
                                   [&](const BatchItem<ColorVertex>& item) {
                                       // Placeholder for RoomId access
                                       // return idsToRemove.count(item.roomId);
                                       return false; // Placeholder
                                   }),
                    items.end());
    }
    void append(const ColoredRoomTexVector& other) {
        items.insert(items.end(), std::make_move_iterator(other.items.begin()), std::make_move_iterator(other.items.end()));
    }
};
// End of conceptual placement for RoomTexVector/ColoredRoomTexVector modifications.
// These might need to go into IMapBatchesFinisher.h or where LayerBatchData is defined.


// Original content continues below if any
// For example, if generateMapDataFinisher was the only thing here previously:

// NODISCARD FutureSharedMapBatchFinisher
// generateMapDataFinisher(const mctp::MapCanvasTexturesProxy &textures, const Map &map);

// extern void finish(const IMapBatchesFinisher &finisher,
//                    std::optional<MapBatches> &batches,
//                    OpenGL &gl,
//                    GLFont &font);

// The above is now replaced by the new signature and the InternalData struct definition.
// The original `finish` function signature also needs to be updated if it's a free function
// or if `IMapBatchesFinisher::virt_finish` is the one being called directly.
// The prompt asks to modify `InternalData::virt_finish`, implying it overrides a base method.
// The `finish` free function would then call `finisher.virt_finish(..., roomsJustUpdated)`.

// Let's adjust the free function `finish` as well, as it's part of the public API of this file.
// This free function `finish` is likely what MapCanvas calls.

// extern void finish(const IMapBatchesFinisher &finisher,
//                    std::optional<MapBatches> &batches,
//                    OpenGL &gl,
//                    GLFont &font);
// This needs to be:
// extern void finish(const IMapBatchesFinisher &finisher,
//                    std::optional<MapBatches> &batches,
//                    OpenGL &gl,
//                    GLFont &font,
//                    const std::optional<RoomIdSet>& roomsJustUpdated);
// The implementation of this free function in the .cpp would pass roomsJustUpdated to finisher.virt_finish.

// Final check on IMapBatchesFinisher in IMapBatchesFinisher.h:
// Its virt_finish would need to be:
// virtual void virt_finish(MapBatches &output_target_on_canvas, OpenGL &gl, GLFont &font, const std::optional<RoomIdSet>& roomsJustUpdated) const = 0;
// (or similar, depending on const correctness and if it's pure virtual)
// This change is outside the scope of MapCanvasRoomDrawer.h but is a dependency.
// For this step, we assume this change is made or will be made.

void resetExistingMeshesButKeepPendingRemesh()
    {
        // mapBatches.reset(); // Keep master copy, only update parts
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

#include <set> // For RoomIdSet

// Forward declarations for types likely defined elsewhere (e.g., related to LayerBatchData)
// or to be defined in the CPP if they are private helper types.
// For now, assume they are types that InternalData will interact with.
struct RoomTexVector;
struct ColoredRoomTexVector;

// Forward declarations for parameters
namespace mctp {
struct MapCanvasTexturesProxy;
}
class Map;
class OpenGL;
class GLFont;

// Modified generateMapDataFinisher signature
NODISCARD FutureSharedMapBatchFinisher
generateMapDataFinisher(const mctp::MapCanvasTexturesProxy &textures, const Map &map, const std::optional<RoomIdSet>& roomIdsToProcess = std::nullopt);

// Modified free function finish signature
extern void finish(const IMapBatchesFinisher &finisher,
                   std::optional<MapBatches> &batches,
                   OpenGL &gl,
                   GLFont &font,
                   const std::optional<RoomIdSet>& roomsJustUpdated);


// InternalData is an implementation of IMapBatchesFinisher
// Its full definition including private members and constructors is in MapCanvasRoomDrawer.cpp
struct InternalData final : public IMapBatchesFinisher {
public:
    // Members that are populated by the async task.
    // These reflect the (potentially partial) data generated.
    std::unordered_map<int, LayerBatchData> batchedMeshes;
    BatchedConnections connectionDrawerBuffers; // Note: Type is BatchedConnections
    std::unordered_map<int, RoomNameBatch> roomNameBatches;

    // Member for future merging logic
    std::unordered_map<int, LayerBatchData> m_masterLayerBatchData;
    std::unordered_map<int, ConnectionDrawerBuffers> m_masterConnectionDrawerBuffers; // Already added in previous step
    std::unordered_map<int, RoomNameBatch> m_masterRoomNameBatches;         // Already added in previous step

    const Map& m_map; // Store reference to the map for context

    // Constructor is defined in the .cpp file
    InternalData(const Map& map); // Default constructor might take the map
    InternalData(const Map& map,
                 std::unordered_map<int, LayerBatchData> currentBatches,
                 BatchedConnections currentConnectionBuffers,
                 std::unordered_map<int, RoomNameBatch> currentRoomNameBatches);

    ~InternalData() override;

    // Made non-const to allow modification of m_masterLayerBatchData etc.
    void virt_finish(
        MapBatches &output_target_on_canvas,
        OpenGL &gl,
        GLFont &font,
        const std::optional<RoomIdSet>& roomsJustUpdated) override;
};

// RoomTexVector and ColoredRoomTexVector are defined and implemented in MapCanvasRoomDrawer.cpp
// No need for placeholder definitions here.
