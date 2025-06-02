#ifndef ASYNC_MAP_AREA_MANAGER_H
#define ASYNC_MAP_AREA_MANAGER_H

#include <string>
#include <future>
#include <vector>
#include <map>
#include <memory> // For std::unique_ptr

// Assuming ConstMapHandle is provided by Map.h or a similar header
// If not, this might need to be const Map*
#include "../map/Map.h" 
#include "./Textures.h" // For MapCanvasTextures
#include "./MapBatches.h" // For MapBatches and potentially its dependencies like Connections.h

// Forward declare OpenGL and GLFont as they might be used in method parameters later,
// but are not members of the structs defined here.
class OpenGL;
class GLFont;

namespace display_async {

struct MapAreaRequestContext {
    std::string areaName;
    ConstMapHandle mapSnapshot; // Or const Map* if ConstMapHandle is not readily available/suitable
    const MapCanvasTextures* textures = nullptr; // Pointer to existing textures
    // Add any other parameters needed to initiate an area remesh,
    // e.g., specific rendering options, if they vary per request and aren't global.
};

// Forward declaration for the PIMPL pattern for AsyncMapAreaIntermediateData
struct AsyncMapAreaIntermediateData_Opaque;

struct AsyncMapAreaIntermediateData {
    // Actual members will be equivalent to `InternalData`'s holdings from MapCanvasRoomDrawer.cpp.
    // Using PIMPL to hide the full definition from this header.
    std::unique_ptr<AsyncMapAreaIntermediateData_Opaque> opaque_data;

    AsyncMapAreaIntermediateData(); // Definition will be in .cpp
    ~AsyncMapAreaIntermediateData(); // Definition will be in .cpp
    AsyncMapAreaIntermediateData(AsyncMapAreaIntermediateData&& other) noexcept; // Definition will be in .cpp
    AsyncMapAreaIntermediateData& operator=(AsyncMapAreaIntermediateData&& other) noexcept; // Definition will be in .cpp

    // No copy operations
    AsyncMapAreaIntermediateData(const AsyncMapAreaIntermediateData&) = delete;
    AsyncMapAreaIntermediateData& operator=(const AsyncMapAreaIntermediateData&) = delete;

    // Helper to check if data is present and valid
    bool isValid() const; // Definition will be in .cpp
};

struct ManagedMapAreaTask {
    std::string areaName_key; // Key for identifying this task/resource
    MapAreaRequestContext originalRequestContext_DEBUG_ONLY; // For re-queueing or debugging

    std::future<AsyncMapAreaIntermediateData> futureAsyncProcess;
    
    // Stores the result from futureAsyncProcess (output of async work, input for main thread finish)
    AsyncMapAreaIntermediateData dataForMainThreadFinish; 

    MapBatches completedBatch; // Final GPU-ready data, output of main thread finish step

    enum class State { 
        IDLE,            // Task is not active or does not exist
        PENDING_ASYNC,   // Asynchronous processing (data generation) is running or scheduled
        PENDING_FINISH,  // Intermediate data is ready, waiting for main thread GPU upload
        COMPLETED,       // GPU upload done, completedBatch is ready for rendering
        FAILED_ASYNC,    // Exception occurred during ProcessAsync (data generation)
        FAILED_FINISH    // Exception occurred during FinishMainThread (GPU upload)
    } state = State::IDLE;

    bool catchUpRequested = false; // True if a new request for this area arrived while it was processing
};

// Forward declarations for classes used as parameters by AsyncMapAreaManager methods
// Note: OpenGL and GLFont were already forward-declared at the top of the file.
// If they were not, this would be the place for them.

class AsyncMapAreaManager {
public:
    AsyncMapAreaManager();
    ~AsyncMapAreaManager();

    AsyncMapAreaManager(const AsyncMapAreaManager&) = delete;
    AsyncMapAreaManager& operator=(const AsyncMapAreaManager&) = delete;
    
    // Defaulted move constructor and assignment operator.
    // std::map and std::mutex handle their own move semantics correctly
    // (mutex is non-movable but that's fine, map is movable).
    AsyncMapAreaManager(AsyncMapAreaManager&&) noexcept = default;
    AsyncMapAreaManager& operator=(AsyncMapAreaManager&&) noexcept = default;

    // Requests an area mesh to be generated or updated.
    void requestAreaMesh(const MapAreaRequestContext& context);

    // Processes completed asynchronous tasks and main thread finishing work.
    // Call this regularly from the main thread.
    // OpenGL and GLFont are passed as references, assuming they are valid during the call.
    void processCompletions(OpenGL& gl, GLFont& font);

    // Retrieves a pointer to the completed MapBatches for an area.
    // Returns nullptr if not found or not yet completed.
    // This method should be const as it doesn't modify the manager's state directly
    // (though it might involve locking a mutable mutex).
    const MapBatches* getCompletedBatch(const std::string& areaName) const;

    // Gets the current state of a resource.
    // This method should be const.
    ManagedMapAreaTask::State getTaskState(const std::string& areaName) const;

private:
    // Direct members as implemented in the .cpp previously
    std::map<std::string, ManagedMapAreaTask> m_managedTasks;
    mutable std::mutex m_mutex; // Mutable to allow locking in const methods like getCompletedBatch/getTaskState
};

} // namespace display_async

#endif // ASYNC_MAP_AREA_MANAGER_H
