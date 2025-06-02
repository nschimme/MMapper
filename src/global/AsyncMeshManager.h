#ifndef ASYNC_RESOURCE_MANAGER_H
#define ASYNC_RESOURCE_MANAGER_H

#include <string>
#include <future>
#include <any>
#include <functional>
#include <memory> // Required for std::unique_ptr if used with generators later
#include <vector> // Required for AsyncMeshManager class member m_readyToFinishKeys
#include <map>    // Required for AsyncMeshManager class members m_generators, m_managedResources
#include <mutex>  // Required for AsyncMeshManager class member m_mutex


namespace async_resource_management {

// Interface for asynchronous resource generators
class IAsyncResourceGenerator {
public:
    virtual ~IAsyncResourceGenerator() = default;

    // Prepares data for the async process.
    // This method is called on the main thread.
    // inputData: Arbitrary data needed for preparation.
    // Returns: Data prepared for ProcessAsync. Can be std::any_cast to expected type.
    virtual std::any PrepareData(std::any inputData) = 0;

    // Core asynchronous work.
    // This method is called on a background thread.
    // preparedData: Data from PrepareData.
    // isCancelled: Function to check if the task has been cancelled.
    // Returns: Intermediate data, result of the async processing.
    virtual std::any ProcessAsync(std::any preparedData, std::function<bool()> isCancelled) = 0;

    // Performs main-thread tasks (e.g., GPU uploads, final object construction).
    // This method is called on the main thread.
    // processedData: Data from ProcessAsync.
    // Returns: The final resource.
    virtual std::any FinishMainThread(std::any processedData /*, potentially a context struct later */) = 0;
};

// Request for resource generation
struct ResourceRequest {
    std::string generatorId;    // To identify which registered generator to use
    std::string resourceKey;    // A unique key for the resource being generated
    std::any inputData;         // Initial data for IAsyncResourceGenerator::PrepareData
    int priority = 0;           // Optional: for future use to prioritize tasks
};

// Represents a managed resource and its state
struct ManagedResource {
    std::string resourceKey;    // The key for this managed resource
    std::string generatorId;    // To know which generator to use for finishing and potential re-requests

    // For debugging, or for simple re-requests if PrepareData is idempotent
    // and inputData is relatively small. Otherwise, re-request logic might be more complex.
    std::any originalInputData_DEBUG_ONLY; 

    std::future<std::any> futureAsyncProcess; // Stores the future from std::async for ProcessAsync

    std::any dataForMainThreadFinish; // Output of ProcessAsync, input for FinishMainThread
    std::any completedResource;       // Output of FinishMainThread, the final product

    enum class State {
        IDLE,             // No current activity or not found
        PENDING_PREPARE,  // Waiting for PrepareData to be called or PrepareData is running
        PENDING_ASYNC,    // ProcessAsync is running or scheduled
        PENDING_FINISH,   // Waiting for FinishMainThread to be called or it is running
        COMPLETED,        // Successfully generated and finished
        FAILED_PREPARE,   // PrepareData failed
        FAILED_ASYNC,     // ProcessAsync failed (or exception caught from future.get())
        FAILED_FINISH     // FinishMainThread failed
    } state = State::IDLE;

    bool catchUpRequested = false; // Flag if a new request for this key came while it was busy
};


// Forward declaration for AsyncResourceManager if it's also in this file
// class AsyncResourceManager; 

// NOTE: The original request asked to append AsyncMeshManager class here.
// It will be renamed to AsyncResourceManager and its members updated to use the new struct/interface names.
// This is handled in a separate step/file or by modifying the existing class definition.
// For now, only the interfaces and structs are redefined as per the prompt.

// The following is the AsyncMeshManager class definition, adapted to new names.
// It was part of the original file and is kept here, modified for the new names.
class AsyncResourceManager { // Renamed from AsyncMeshManager
public:
    AsyncResourceManager();
    ~AsyncResourceManager();

    void registerGenerator(const std::string& generatorId, std::unique_ptr<IAsyncResourceGenerator> generator);
    void requestResource(const ResourceRequest& request);
    void processAsyncCompletions();
    void processMainThreadFinishers();
    std::any getResource(const std::string& resourceKey) const;
    ManagedResource::State getResourceState(const std::string& resourceKey) const;

private:
    template<typename T>
    bool is_future_ready(const std::future<T>& future) const;

    std::map<std::string, std::unique_ptr<IAsyncResourceGenerator>> m_generators;
    std::map<std::string, ManagedResource> m_managedResources; // Changed from ManagedMesh
    mutable std::mutex m_mutex;
    std::vector<std::string> m_readyToFinishKeys;
};


} // namespace async_resource_management

#endif // ASYNC_RESOURCE_MANAGER_H
