#include "AsyncMapAreaManager.h"

// Includes for ported logic
#include "../common/Configuration.h" // For cfg()
#include "../common/LColor.h"        // For LColor
#include "../common/Quad.h"          // For Quad (used by original code)
#include "../common/Vec.h"           // For Vec2/Vec3
#include "../gl/Font.h"              // For FontFormatFlags, GLFont (used by processCompletions)
#include "../gl/OpenGL.h"            // For OpenGL (used by processCompletions)
#include "../gl/Texture.h"         // For Texture definition
#include "../map/Map.h"              // For Map, ConstMapHandle
#include "../map/MapArea.h"          // For MapArea
#include "../map/MapConnection.h"    // For MapConnection (for connection data)
#include "../map/Room.h"             // For Room, RoomHandle
#include "../map/RoomTile.h"         // For RoomTile, TILE_SIZE etc.
#include "./TexturesProxy.h"       // For MapCanvasTexturesProxy
#include "./TextureFlags.h"        // For TextureFlags
// #include "./MapCanvasRoomDrawer.h" // Original source - NOT included directly
// #include "./TextureFont.h"      // For TextureFont - GLFont used for text rendering finish step

#include <algorithm> // For std::remove_if, std::any_of, std::sort, std::unique
#include <array>
#include <functional> // for std::function
#include <iostream>  // For placeholder logging
#include <vector>
#include <algorithm> // For std::remove_if

#include <iostream>  // For placeholder logging
#include <map>       // For std::map
#include <memory>    // For std::unique_ptr
#include <mutex>     // For std::mutex, std::lock_guard
#include <stdexcept> // For std::runtime_error
#include <string>    // For std::string
#include <vector>    // For std::vector
#include <variant>   // For std::variant if used in RoomTex/ColoredRoomTex

// Constants from original context (ensure these are appropriate)
constexpr float DEFAULT_Z_VALUE = 0.0f;
constexpr float ROOM_TILE_SHAPE_Z_OFFSET = 0.001f;
constexpr float Z_FIGHT_INCREMENT = 0.0001f; // Original was Z_FIGHT_2D_INCREMENT
constexpr int ROOM_TILE_SIZE = 16; // Assuming this is the definition from common/Constants.h or map/RoomTile.h

namespace { // Anonymous namespace for internal helpers and data structures

// --- Ported/Adapted Data Structures ---
// (Using Vec2f, Vec3f, QuadF directly as they are from common)

// Represents a textured quad vertex with room context
struct RoomTex {
    Vec3f pos[4];
    Vec2f tex[4];
    const Texture* texture_id = nullptr;
    RoomHandle room = nullptr;

    RoomTex(RoomHandle r, const Texture* tex_id, const QuadF& q, const QuadF& tq) : texture_id(tex_id), room(r) {
        for (int i = 0; i < 4; ++i) {
            pos[i] = q.points[i]; // Assumes QuadF points are Vec3f or convertible
            tex[i] = tq.points[i]; // Assumes QuadF points are Vec2f or convertible
        }
    }
     // Constructor for individual points
    RoomTex(RoomHandle r, const Texture* tex_id,
            const Vec3f& p0, const Vec3f& p1, const Vec3f& p2, const Vec3f& p3,
            const Vec2f& t0, const Vec2f& t1, const Vec2f& t2, const Vec2f& t3)
        : texture_id(tex_id), room(r) {
        pos[0] = p0; pos[1] = p1; pos[2] = p2; pos[3] = p3;
        tex[0] = t0; tex[1] = t1; tex[2] = t2; tex[3] = t3;
    }
};

// Represents a colored, textured quad vertex with room context
struct ColoredRoomTex {
    Vec3f pos[4];
    Vec2f tex[4];
    LColor color;
    const Texture* texture_id = nullptr; // Usually white pixel texture
    RoomHandle room = nullptr;

    ColoredRoomTex(RoomHandle r, const Texture* tex_id, LColor c, const QuadF& q, const QuadF& tq)
        : color(c), texture_id(tex_id), room(r) {
        for (int i = 0; i < 4; ++i) {
            pos[i] = q.points[i];
            tex[i] = tq.points[i];
        }
    }
     ColoredRoomTex(RoomHandle r, const Texture* tex_id, LColor c,
                   const Vec3f& p0, const Vec3f& p1, const Vec3f& p2, const Vec3f& p3,
                   const Vec2f& t0, const Vec2f& t1, const Vec2f& t2, const Vec2f& t3)
        : color(c), texture_id(tex_id), room(r) {
        pos[0] = p0; pos[1] = p1; pos[2] = p2; pos[3] = p3;
        tex[0] = t0; tex[1] = t1; tex[2] = t2; tex[3] = t3;
    }
};

using RoomTexVector = std::vector<RoomTex>;
using ColoredRoomTexVector = std::vector<ColoredRoomTex>;

// This is the pre-GPU data structure, replacing the placeholder `AdaptedLayerBatchData`
struct RichLayerBatchData {
    RoomTexVector terrain;
    RoomTexVector overlays; // Combined overlays, special effects, etc.
    RoomTexVector trails;
    ColoredRoomTexVector walls; // Includes pillars, door frames if using same shader
    ColoredRoomTexVector doors;
    ColoredRoomTexVector floors;   // For specific per-room floor colors
    ColoredRoomTexVector ceilings; // For specific per-room ceiling colors
    // Could add more categories if needed, e.g., water, furniture outlines
    bool has_data = false;

    // Helper methods to add data
    void addTerrain(RoomTex&& rt) { terrain.emplace_back(std::move(rt)); has_data = true; }
    void addOverlay(RoomTex&& rt) { overlays.emplace_back(std::move(rt)); has_data = true; }
    void addTrail(RoomTex&& rt) { trails.emplace_back(std::move(rt)); has_data = true; }
    void addWall(ColoredRoomTex&& crt) { walls.emplace_back(std::move(crt)); has_data = true; }
    void addDoor(ColoredRoomTex&& crt) { doors.emplace_back(std::move(crt)); has_data = true; }
    void addFloor(ColoredRoomTex&& crt) { floors.emplace_back(std::move(crt)); has_data = true; }
    void addCeiling(ColoredRoomTex&& crt) { ceilings.emplace_back(std::move(crt)); has_data = true; }
};

// Pre-GPU structure for room names.
struct RichRoomNameData {
    std::string text;
    Vec3f position; // Center or baseline start of the text
    FontFormatFlags format_flags = FONT_NONE;
    LColor color = Colors::WHITE;
    float scale = 1.0f;
    RoomHandle room = nullptr; // For context
};

// This replaces placeholder `AdaptedRoomNameBatch`
struct RichRoomNameBatch {
    std::vector<RichRoomNameData> names;
    bool has_data = false;
    void addName(RichRoomNameData&& name_data) { names.emplace_back(std::move(name_data)); has_data = true; }
};


// --- Ported Room Visitor Logic ---
// (Minimal stubs for IRoomVisitorCallbacks and LayerBatchBuilder for now,
//  the real logic will be inside PopulateIntermediateMapData directly or via helpers)

// Define the actual opaque data structure for AsyncMapAreaIntermediateData
struct AsyncMapAreaIntermediateData_Opaque {
    std::map<int, RichLayerBatchData> layer_batches;      // Keyed by layer index
    display_async::BatchedConnections connection_batches; // From display_async namespace
    std::map<int, RichRoomNameBatch> room_name_batches;   // Keyed by layer index
    bool hasData = false;
};


// --- Main Data Population Function ---
// This function will encapsulate the logic adapted from MapCanvasRoomDrawer::generateAllLayerMeshes
// and its helpers. For this step, it will be a high-level placeholder.
// The actual porting of visitRoom, etc. will be done incrementally.

AsyncMapAreaIntermediateData PopulateIntermediateMapData(
    ConstMapHandle map_handle,
    const MapCanvasTextures* textures_raw,
    const std::string& area_name) {

    AsyncMapAreaIntermediateData result_data;
    result_data.opaque_data->hasData = false; // Default to no data

    if (!map_handle || !textures_raw) {
        std::cerr << "PopulateIntermediateMapData: Null map_handle or textures_raw." << std::endl;
        return result_data;
    }

    const MapArea* area = map_handle->getAreaByName(area_name);
    if (!area) {
        std::cerr << "PopulateIntermediateMapData: Area '" << area_name << "' not found." << std::endl;
        return result_data;
    }

    MapCanvasTexturesProxy textures_proxy(textures_raw, map_handle.get());
    bool any_data_generated = false;

    // --- 1. Populate Layer Batches (Simplified for this step) ---
    std::cout << "PopulateIntermediateMapData: Processing layers for area " << area_name << std::endl;
    for (int layer_idx = area->getMinLevel(); layer_idx <= area->getMaxLevel(); ++layer_idx) {
        RichLayerBatchData current_layer_batch_data;
        float current_z_offset = static_cast<float>(layer_idx) * Z_FIGHT_INCREMENT * 10.0f; // Example Z

        for (const auto& room_ptr : area->getRooms()) {
            RoomHandle room = room_ptr.get();
            if (room->getLevel() != layer_idx) continue;

            // Simplified terrain quad for each room
            const Texture* terrain_tex = textures_proxy.getTerrainTexture(room->getTerrainId());
            if (terrain_tex) {
                float r_x = static_cast<float>(room->getX() * ROOM_TILE_SIZE);
                float r_y = static_cast<float>(room->getY() * ROOM_TILE_SIZE);
                float r_w = static_cast<float>(room->getWidth() * ROOM_TILE_SIZE);
                float r_h = static_cast<float>(room->getHeight() * ROOM_TILE_SIZE);
                Vec3f p0(r_x, r_y, current_z_offset);
                Vec3f p1(r_x + r_w, r_y, current_z_offset);
                Vec3f p2(r_x + r_w, r_y + r_h, current_z_offset);
                Vec3f p3(r_x, r_y + r_h, current_z_offset);
                current_layer_batch_data.addTerrain(RoomTex(room, terrain_tex, p0,p1,p2,p3, Vec2f(0,0),Vec2f(1,0),Vec2f(1,1),Vec2f(0,1)));
                any_data_generated = true;
            }
            // In a full version: iterate tiles for walls, doors, overlays, trails, etc.
            // This would involve adapting the logic from the original visitRoom/LayerBatchBuilder.
        }
        if (current_layer_batch_data.has_data) {
            result_data.opaque_data->layer_batches[layer_idx] = std::move(current_layer_batch_data);
        }
    }
    
    // --- 2. Populate Connection Batches (Placeholder) ---
    std::cout << "PopulateIntermediateMapData: Processing connections (placeholder) for area " << area_name << std::endl;
    // display_async::BatchedConnections connections_data; // member of opaque_data
    // Iterate map_handle->getConnections(), filter by area, generate pre-GPU line data.
    // For now, result_data.opaque_data->connection_batches remains default-constructed (empty).
    // if (!connections_data.empty()) any_data_generated = true;


    // --- 3. Populate Room Name Batches (Placeholder) ---
    std::cout << "PopulateIntermediateMapData: Processing room names (placeholder) for area " << area_name << std::endl;
    for (int layer_idx = area->getMinLevel(); layer_idx <= area->getMaxLevel(); ++layer_idx) {
        RichRoomNameBatch current_room_name_batch;
        float current_z_offset = static_cast<float>(layer_idx) * Z_FIGHT_INCREMENT * 10.0f + ROOM_NAME_DEPTH;
        for (const auto& room_ptr : area->getRooms()) {
            RoomHandle room = room_ptr.get();
            if (room->getLevel() != layer_idx || room->getName().empty()) continue;
            
            // Example of adding a room name - position needs to be calculated correctly (e.g. center)
            Vec3f name_pos(
                static_cast<float>(room->getX() * ROOM_TILE_SIZE + (room->getWidth() * ROOM_TILE_SIZE) / 2.0f),
                static_cast<float>(room->getY() * ROOM_TILE_SIZE + (room->getHeight() * ROOM_TILE_SIZE) / 2.0f),
                current_z_offset
            );
            current_room_name_batch.addName({room->getName(), name_pos, FONT_CENTER | FONT_VCENTER, Colors::WHITE, 1.0f, room});
            any_data_generated = true;
        }
         if (current_room_name_batch.has_data) {
            result_data.opaque_data->room_name_batches[layer_idx] = std::move(current_room_name_batch);
        }
    }

    result_data.opaque_data->hasData = any_data_generated;
    std::cout << "PopulateIntermediateMapData finished for area: " << area_name << ". Has Data: " << result_data.opaque_data->hasData << std::endl;
    return result_data;
}

} // namespace

namespace display_async {

// --- AsyncMapAreaIntermediateData PIMPL Implementation ---
// Constructor, Destructor, Move ops, isValid remain the same but operate on the new AsyncMapAreaIntermediateData_Opaque
AsyncMapAreaIntermediateData::AsyncMapAreaIntermediateData() 
    : opaque_data(std::make_unique<AsyncMapAreaIntermediateData_Opaque>()) {
    if (opaque_data) {
        opaque_data->hasData = false; 
    }
}

// --- AsyncMapAreaIntermediateData PIMPL Implementation ---
AsyncMapAreaIntermediateData::AsyncMapAreaIntermediateData() 
    : opaque_data(std::make_unique<AsyncMapAreaIntermediateData_Opaque>()) {
    if (opaque_data) {
        opaque_data->hasData = false; 
    }
}

AsyncMapAreaIntermediateData::~AsyncMapAreaIntermediateData() = default;

AsyncMapAreaIntermediateData::AsyncMapAreaIntermediateData(AsyncMapAreaIntermediateData&& other) noexcept 
    : opaque_data(std::move(other.opaque_data)) {
}

AsyncMapAreaIntermediateData& AsyncMapAreaIntermediateData::operator=(AsyncMapAreaIntermediateData&& other) noexcept {
    if (this != &other) {
        opaque_data = std::move(other.opaque_data);
    }
    return *this;
}

bool AsyncMapAreaIntermediateData::isValid() const {
    return opaque_data && opaque_data->hasData;
}


// --- AsyncMapAreaManager Method Implementations ---

// --- Static Helper Functions ---
template<typename T>
static bool is_future_ready(const std::future<T>& future) {
    if (!future.valid()) {
        return false;
    }
    return future.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
}

// Constructor and Destructor implementations (can be defaulted if simple)
AsyncMapAreaManager::AsyncMapAreaManager() = default;
AsyncMapAreaManager::~AsyncMapAreaManager() = default;

// Move constructor and assignment operator (can be defaulted)
// AsyncMapAreaManager::AsyncMapAreaManager(AsyncMapAreaManager&&) noexcept = default;
// AsyncMapAreaManager& AsyncMapAreaManager::operator=(AsyncMapAreaManager&&) noexcept = default;


// Requests an area mesh to be generated or updated.
void AsyncMapAreaManager::requestAreaMesh(const MapAreaRequestContext& context) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::cout << "Requesting area mesh for: " << context.areaName << std::endl;

    auto& task = m_managedTasks[context.areaName];
    task.areaName_key = context.areaName;
    task.originalRequestContext_DEBUG_ONLY = context; // Store for potential re-queue

    if (task.state == ManagedMapAreaTask::State::PENDING_ASYNC || 
        task.state == ManagedMapAreaTask::State::PENDING_FINISH) {
        std::cout << "Area " << context.areaName << " is already processing. Setting catchUpRequested." << std::endl;
        task.catchUpRequested = true;
        return;
    }

    task.state = ManagedMapAreaTask::State::PENDING_ASYNC;
    task.completedBatch = MapBatches(); // Clear previous result
    task.dataForMainThreadFinish = AsyncMapAreaIntermediateData(); // Clear previous intermediate data
    task.catchUpRequested = false;

    // Capture context by value for the async lambda
    task.futureAsyncProcess = std::async(std::launch::async, 
        [ctx = context]() -> AsyncMapAreaIntermediateData {
            std::cout << "Async task started for area: " << ctx.areaName << " using AdaptedGenerateAllMapAreaVisuals." << std::endl;
            // Call the adapted data generation function
            AsyncMapAreaIntermediateData result = AdaptedGenerateAllMapAreaVisuals(
                ctx.mapSnapshot, 
                ctx.textures, 
                ctx.areaName
            );
            std::cout << "Async task finished for area: " << ctx.areaName << ". Data valid: " << result.isValid() << std::endl;
            return result;
        });
}

// --- GPU Upload Logic ---

// Helper to create a textured quad batch from RoomTexVector
gl::Mesh createTexturedQuadMeshFromRoomTexVector(OpenGL& gl, const RoomTexVector& vec, const Texture* default_texture = nullptr) {
    if (vec.empty()) return gl::Mesh();
    std::vector<gl:: TexturedQuad::Vertex> vertices;
    vertices.reserve(vec.size() * 4);
    const Texture* current_texture = default_texture;
    if (!vec.empty()) { // Get texture from first element if possible
        current_texture = vec[0].texture_id ? vec[0].texture_id : default_texture;
    }


    for (const auto& rt : vec) {
        // Assuming all quads in this specific vector use the same texture,
        // or that createTexturedQuadBatch handles texture changes (which it typically doesn't, batch per texture).
        // For simplicity, this helper assumes one texture per call or that `rt.texture_id` is consistent.
        // If not, `UploadIntermediateDataToGpu` needs to group by texture before calling.
        if(rt.texture_id) current_texture = rt.texture_id; // Update if RoomTex specifies one

        for (int i = 0; i < 4; ++i) {
            vertices.emplace_back(gl::TexturedQuad::Vertex{rt.pos[i], rt.tex[i]});
        }
    }
    if (!current_texture) {
         //This case should be handled: either error or use a placeholder like white pixel.
         //For now, returning empty mesh if no texture.
        return gl::Mesh();
    }
    return gl.createTexturedQuadBatch(vertices, current_texture);
}

// Helper to create a colored textured quad batch from ColoredRoomTexVector
gl::Mesh createColoredTexturedQuadMeshFromColoredRoomTexVector(OpenGL& gl, const ColoredRoomTexVector& vec, const Texture* default_texture = nullptr) {
    if (vec.empty()) return gl::Mesh();
    std::vector<gl::ColoredTexturedQuad::Vertex> vertices;
    vertices.reserve(vec.size() * 4);
    const Texture* current_texture = default_texture;
     if (!vec.empty()) {
        current_texture = vec[0].texture_id ? vec[0].texture_id : default_texture;
    }


    for (const auto& crt : vec) {
        if(crt.texture_id) current_texture = crt.texture_id;

        for (int i = 0; i < 4; ++i) {
            vertices.emplace_back(gl::ColoredTexturedQuad::Vertex{crt.pos[i], crt.tex[i], crt.color});
        }
    }
     if (!current_texture) {
        return gl::Mesh(); // Or handle error / placeholder texture
    }
    return gl.createColoredTexturedQuadBatch(vertices, current_texture);
}


static MapBatches UploadIntermediateDataToGpu(
    const AsyncMapAreaIntermediateData& intermediateData,
    OpenGL& gl,
    GLFont& font,
    const MapCanvasTextures* map_textures, // For white pixel texture etc.
    const std::string& areaName_key) {

    MapBatches result_batch;
    std::cout << "UploadIntermediateDataToGpu: Starting GPU upload for area: " << areaName_key << std::endl;

    if (!intermediateData.isValid() || !intermediateData.opaque_data) {
        std::cerr << "UploadIntermediateDataToGpu: Intermediate data is invalid or null for area " << areaName_key << std::endl;
        return result_batch;
    }

    const auto& opaque = *intermediateData.opaque_data;
    const Texture* white_pixel_tex = map_textures ? map_textures->getWhitePixel() : nullptr;
    if (!white_pixel_tex) {
        std::cerr << "UploadIntermediateDataToGpu: White pixel texture is null. Cannot proceed with some batches." << std::endl;
        // Depending on strictness, might return empty result_batch here or try to proceed.
    }


    if (opaque.hasData) {
        // 1. Process Layer Batches
        for (const auto& [layer_idx, layer_data] : opaque.layer_batches) {
            if (!layer_data.has_data) continue;
            std::cout << "UploadIntermediateDataToGpu: Uploading layer " << layer_idx << " for " << areaName_key << std::endl;
            
            LayerMeshes& gpu_layer_meshes = result_batch.layers[layer_idx]; // Create/get layer mesh entry

            // Terrain
            gpu_layer_meshes.roomTerrain = createTexturedQuadMeshFromRoomTexVector(gl, layer_data.terrain);
            // Overlays
            gpu_layer_meshes.roomOverlay = createTexturedQuadMeshFromRoomTexVector(gl, layer_data.overlays);
            // Trails
            gpu_layer_meshes.roomTrail = createTexturedQuadMeshFromRoomTexVector(gl, layer_data.trails);
            
            // Walls (assuming they use white_pixel_tex if no specific texture)
            gpu_layer_meshes.roomWalls = createColoredTexturedQuadMeshFromColoredRoomTexVector(gl, layer_data.walls, white_pixel_tex);
            // Doors
            gpu_layer_meshes.roomDoors = createColoredTexturedQuadMeshFromColoredRoomTexVector(gl, layer_data.doors, white_pixel_tex);
            // Floors
            gpu_layer_meshes.roomFloors = createColoredTexturedQuadMeshFromColoredRoomTexVector(gl, layer_data.floors, white_pixel_tex);
            // Ceilings
            gpu_layer_meshes.roomCeilings = createColoredTexturedQuadMeshFromColoredRoomTexVector(gl, layer_data.ceilings, white_pixel_tex);
            
            // TODO: Add other categories from RichLayerBatchData as needed (e.g. upDownExits, streamIns/Outs, tints etc.)
            // These would follow the same pattern:
            // gpu_layer_meshes.tints = createColoredTexturedQuadMeshFromColoredRoomTexVector(gl, layer_data.tints, white_pixel_tex);
            // gpu_layer_meshes.roomShapes = createColoredTexturedQuadMeshFromColoredRoomTexVector(gl, layer_data.roomShapes, white_pixel_tex);
        }

        // 2. Process Connection Batches
        std::cout << "UploadIntermediateDataToGpu: Uploading connections for " << areaName_key << std::endl;
        if (!opaque.connection_batches.empty()) {
            // display_async::BatchedConnections is std::map<int, ConnectionDrawerBuffers>
            // ConnectionDrawerBuffers is std::vector<ConnectionVertexData>
            // ConnectionVertexData is { Vec3f pos; LColor color; }
            // We need to convert this to something createLineBatchList can use, or adapt createLineBatchList.
            // Assuming gl.createLineBatchList takes std::vector<ConnectionVertexData> directly for each buffer.
            for(const auto& [type, buffer] : opaque.connection_batches){
                if(!buffer.vertices.empty()){
                     // MapBatches.connections is a BatchedConnections type (map<int, MeshList>)
                     // This might need adjustment based on how BatchedConnections in MapBatches is structured.
                     // If MapBatches::connections is also a map<int, ConnectionDrawerBuffers> or similar,
                     // then we'd store the raw data. But it's likely map<int, MeshList> or Mesh.
                     // For now, assuming one combined mesh for all connections or per type.
                     // This part is simplified as the exact structure of MapBatches::connections
                     // and gl.createLineBatchList needs to be aligned.
                     // Let's assume we create one line list for all connections for now.
                }
            }
            // Placeholder: result_batch.connections.main_lines = gl.createLineBatchList(...);
            // This needs a proper loop and conversion if BatchedConnections holds multiple buffers.
            // For now, this part remains largely conceptual until ConnectionVertexData structure is used by GL.
            std::cout << "Connections data present but GPU upload logic is placeholder." << std::endl;
        }


        // 3. Process Room Name Batches
         std::cout << "UploadIntermediateDataToGpu: Uploading room names for " << areaName_key << std::endl;
        for (const auto& [layer_idx, name_batch_data] : opaque.room_name_batches) {
            if (!name_batch_data.has_data || name_batch_data.names.empty()) continue;

            // MapBatches.roomNames is RoomNameMeshes, which is std::map<int, std::map<FontFormatFlags, FontMesh3d>>
            std::map<FontFormatFlags, FontMesh3d>& layer_room_names = result_batch.roomNames[layer_idx];

            for (const auto& name_info : name_batch_data.names) {
                FontMesh3d mesh = font.getMesh(name_info.text, name_info.format_flags, name_info.scale);
                // Need to set position and color for this mesh if not part of getMesh
                // Assuming FontMesh3d can be transformed or holds this info.
                // The original code might create a batch per format flag.
                // For now, this is simplified. A proper implementation would group FontMesh3d by format flags.
                if(mesh.getVao() != 0) { // Check if mesh is valid
                    // This is not how FontMesh3d is typically stored in RoomNameMeshes.
                    // RoomNameMeshes usually stores one FontMesh3d *per set of formatting flags*.
                    // The current loop creates one mesh per name_info. This needs to be aggregated.
                    // For now, as a placeholder, let's assume we are just testing one name or the first one.
                    if (layer_room_names.find(name_info.format_flags) == layer_room_names.end()) {
                         layer_room_names[name_info.format_flags] = std::move(mesh);
                         // TODO: Apply position and color to the FontMesh3d instance or its rendering.
                         // The current FontMesh3d doesn't store this. This happens at render time.
                         // So, RichRoomNameData (text, pos, color, flags, scale) should be stored in MapBatches instead of FontMesh3d directly.
                         // OR MapBatches stores FontMesh3d and a list of transformations for each text instance.
                         // This part needs significant reconciliation with how MapBatches and GLFont render.
                         // For now, the fact that we *would* call getMesh is the placeholder.
                    }
                }
            }
             std::cout << "Room name data for layer " << layer_idx << " processed (placeholder for actual storage in MapBatches)." << std::endl;
        }

    } else {
        std::cout << "UploadIntermediateDataToGpu: No opaque data to upload for area " << areaName_key << std::endl;
    }
    
    std::cout << "UploadIntermediateDataToGpu completed for area: " << areaName_key << std::endl;
    return result_batch;
}


// Processes completed asynchronous tasks and main thread finishing work.
void AsyncMapAreaManager::processCompletions(OpenGL& gl, GLFont& font) {
    std::vector<std::string> keys_to_process_finish_stage;
    std::vector<std::string> keys_to_check_async_completion;

        {
            std::lock_guard<std::mutex> lock(m_mutex); // Accessing m_managedTasks
            for (auto const& [key, task] : m_managedTasks) {
                if (task.state == ManagedMapAreaTask::State::PENDING_ASYNC) {
                    keys_to_check_async_completion.push_back(key);
                } else if (task.state == ManagedMapAreaTask::State::PENDING_FINISH) {
                    keys_to_process_finish_stage.push_back(key);
                }
            }
        } // Release main lock

        std::vector<MapAreaRequestContext> requests_for_catch_up;

        // Check for async completions
        for (const std::string& key : keys_to_check_async_completion) {
            std::lock_guard<std::mutex> lock(m_mutex); // Re-lock for individual task modification
            auto it = m_managedTasks.find(key);
            if (it == m_managedTasks.end() || it->second.state != ManagedMapAreaTask::State::PENDING_ASYNC) {
                continue; // Task state changed or removed
            }
            ManagedMapAreaTask& task = it->second;

            if (task.futureAsyncProcess.valid() && is_future_ready(task.futureAsyncProcess)) {
                try {
                    task.dataForMainThreadFinish = task.futureAsyncProcess.get();
                    if (task.dataForMainThreadFinish.isValid()) {
                        task.state = ManagedMapAreaTask::State::PENDING_FINISH;
                        keys_to_process_finish_stage.push_back(key); // Add to list for finishing in this same cycle
                        std::cout << "Async data ready for area: " << key << ". Moved to PENDING_FINISH." << std::endl;
                    } else {
                        task.state = ManagedMapAreaTask::State::FAILED_ASYNC;
                         std::cerr << "Async data processing returned invalid data for area: " << key << std::endl;
                    }
                } catch (const std::exception& e) {
                    task.state = ManagedMapAreaTask::State::FAILED_ASYNC;
                    std::cerr << "Exception during async processing for area " << key << ": " << e.what() << std::endl;
                } catch (...) {
                    task.state = ManagedMapAreaTask::State::FAILED_ASYNC;
                    std::cerr << "Unknown exception during async processing for area " << key << std::endl;
                }
            }
        }
        
        // Process tasks ready for main-thread finishing
        // Sort and unique to avoid processing a task twice if it was added from PENDING_ASYNC check
        std::sort(keys_to_process_finish_stage.begin(), keys_to_process_finish_stage.end());
        keys_to_process_finish_stage.erase(std::unique(keys_to_process_finish_stage.begin(), keys_to_process_finish_stage.end()), keys_to_process_finish_stage.end());


        for (const std::string& key : keys_to_process_finish_stage) {
            MapAreaRequestContext catch_up_context;
            bool needs_catch_up = false;
            AsyncMapAreaIntermediateData data_to_finish;
            std::string area_key_for_upload; // To pass to UploadIntermediateDataToGpu for logging

            {
                std::lock_guard<std::mutex> lock(m_mutex); 
                auto it = m_managedTasks.find(key);
                if (it == m_managedTasks.end() || it->second.state != ManagedMapAreaTask::State::PENDING_FINISH) {
                    continue; 
                }
                ManagedMapAreaTask& task = it->second;
                data_to_finish = std::move(task.dataForMainThreadFinish); 
                area_key_for_upload = task.areaName_key; // Copy key for logging outside lock
            } 

            MapBatches result_batch; 
            bool finish_successful = true; 
            try {
                // Call the GPU upload function
                result_batch = UploadIntermediateDataToGpu(data_to_finish, gl, font, area_key_for_upload);
                // `finish_successful` remains true if no exception is thrown.
                // The actual success (e.g. if MapBatches is valid) could be checked here if UploadIntermediateDataToGpu indicated it.
            } catch (const std::exception& e) {
                std::cerr << "Exception during UploadIntermediateDataToGpu for area " << area_key_for_upload << ": " << e.what() << std::endl;
                finish_successful = false;
            } catch (...) {
                std::cerr << "Unknown exception during UploadIntermediateDataToGpu for area " << area_key_for_upload << std::endl;
                finish_successful = false;
            }

            { 
                std::lock_guard<std::mutex> lock(m_mutex);
                auto it = m_managedTasks.find(key);
                if (it == m_managedTasks.end()) continue; 
                ManagedMapAreaTask& task = it->second;

                if(finish_successful) {
                    task.completedBatch = std::move(result_batch);
                    task.state = ManagedMapAreaTask::State::COMPLETED;
                    std::cout << "Area " << key << " successfully processed and completed." << std::endl;
                } else {
                    task.state = ManagedMapAreaTask::State::FAILED_FINISH;
                }

                if (task.catchUpRequested) {
                    needs_catch_up = true;
                    catch_up_context = task.originalRequestContext_DEBUG_ONLY;
                    task.catchUpRequested = false; 
                }
            }
             if (needs_catch_up) {
                requests_for_catch_up.push_back(catch_up_context);
            }
        }

        if (!requests_for_catch_up.empty()) {
            std::cout << "Processing " << requests_for_catch_up.size() << " catch-up requests." << std::endl;
            for (const auto& ctx : requests_for_catch_up) {
                requestAreaMesh(ctx); 
            }
        }
    }

    const MapBatches* AsyncMapAreaManager::getCompletedBatch(const std::string& areaName) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_managedTasks.find(areaName);
        if (it != m_managedTasks.end() && it->second.state == ManagedMapAreaTask::State::COMPLETED) {
            return &it->second.completedBatch;
        }
        return nullptr;
    }
    
    ManagedMapAreaTask::State AsyncMapAreaManager::getTaskState(const std::string& areaName) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_managedTasks.find(areaName);
        if (it != m_managedTasks.end()) {
            return it->second.state;
        }
        return ManagedMapAreaTask::State::IDLE; 
    }

// Private methods like finishTask would be defined here if they were part of the class
// For example:
// MapBatches AsyncMapAreaManager::finishTask(AsyncMapAreaIntermediateData& intermediateData, OpenGL& gl, GLFont& font) { ... }

} // namespace display_async
