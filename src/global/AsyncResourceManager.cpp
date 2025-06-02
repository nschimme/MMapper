#include "AsyncMeshManager.h" // Will be AsyncResourceManager.h post-change
#include <stdexcept> // For std::runtime_error
#include <iostream>  // For placeholder logging (replace with actual logging later)
#include <vector>    // Required for m_readyToFinishKeys and local key_to_process

namespace async_resource_management {

AsyncResourceManager::AsyncResourceManager() = default; // Renamed class
AsyncResourceManager::~AsyncResourceManager() { // Renamed class
    // Consider how to handle ongoing tasks: cancel, wait?
    // For now, futures will detach if manager is destroyed, which might lead to
    // std::terminate if the future is not yet ready and its destructor is called.
    // A more robust solution would involve joining threads or explicitly cancelling.
}

void AsyncResourceManager::registerGenerator(const std::string& generatorId, std::unique_ptr<IAsyncResourceGenerator> generator) { // Renamed class and interface
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_generators.count(generatorId)) {
        // Potentially log a warning: overwriting an existing generator
        std::cout << "Warning: Overwriting generator with ID '" << generatorId << "'." << std::endl;
    }
    m_generators[generatorId] = std::move(generator);
}

template<typename T>
bool AsyncResourceManager::is_future_ready(const std::future<T>& future) const { // Renamed class
    if (!future.valid()) {
        return false;
    }
    // wait_for with zero duration is a non-blocking check.
    return future.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
}

void AsyncResourceManager::requestResource(const ResourceRequest& request) { // Renamed class and struct
    std::lock_guard<std::mutex> lock(m_mutex);

    auto gen_it = m_generators.find(request.generatorId);
    if (gen_it == m_generators.end()) {
        std::cerr << "Error: Generator with ID '" << request.generatorId << "' not found for resource '" << request.resourceKey << "'." << std::endl;
        // m_managedResources[request.resourceKey].state = ManagedResource::State::FAILED_PREPARE; // Ensure state is FAILED_PREPARE
        return;
    }
    IAsyncResourceGenerator* generator = gen_it->second.get(); // Renamed interface

    auto& resource = m_managedResources[request.resourceKey]; // Creates if not exists, type is now ManagedResource
    resource.resourceKey = request.resourceKey;
    resource.generatorId = request.generatorId; // Store generatorId
    resource.originalInputData_DEBUG_ONLY = request.inputData; // Store inputData for debug/re-request

    if (resource.state == ManagedResource::State::PENDING_ASYNC || 
        resource.state == ManagedResource::State::PENDING_PREPARE || 
        resource.state == ManagedResource::State::PENDING_FINISH) {
        resource.catchUpRequested = true;
        std::cout << "Info: Resource " << request.resourceKey << " is busy. Catch-up requested." << std::endl;
        return;
    }

    resource.state = ManagedResource::State::PENDING_PREPARE;
    resource.completedResource = std::any(); // Clear previous resource
    resource.catchUpRequested = false;
    resource.dataForMainThreadFinish = std::any(); // Clear previous intermediate data

    try {
        std::cout << "Info: Preparing data for " << request.resourceKey << std::endl;
        std::any prepared_data = generator->PrepareData(request.inputData);
        
        resource.state = ManagedResource::State::PENDING_ASYNC;
        resource.futureAsyncProcess = std::async(std::launch::async, // Renamed member: future -> futureAsyncProcess
                                     [generator, prep_data = std::move(prepared_data), key = request.resourceKey]() -> std::any {
            std::cout << "Info: Starting ProcessAsync for " << key << std::endl;
            std::any result = generator->ProcessAsync(prep_data, []{ return false; }); // TODO: Cancellation
            std::cout << "Info: Finished ProcessAsync for " << key << std::endl;
            return result;
        });
        std::cout << "Info: Async task started for " << request.resourceKey << std::endl;
    } catch (const std::exception& e) {
        resource.state = ManagedResource::State::FAILED_PREPARE;
        std::cerr << "Error during PrepareData for " << request.resourceKey << ": " << e.what() << std::endl;
    } catch (...) {
        resource.state = ManagedResource::State::FAILED_PREPARE;
        std::cerr << "Unknown error during PrepareData for " << request.resourceKey << std::endl;
    }
}

void AsyncResourceManager::processAsyncCompletions() { // Renamed class
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& pair : m_managedResources) {
        ManagedResource& resource = pair.second; // Type is now ManagedResource
        if (resource.state == ManagedResource::State::PENDING_ASYNC) {
            if (!resource.futureAsyncProcess.valid()) { // Renamed member: future -> futureAsyncProcess
                 std::cerr << "Error: Invalid future for " << resource.resourceKey << " in PENDING_ASYNC." << std::endl;
                 resource.state = ManagedResource::State::FAILED_ASYNC;
                 continue;
            }
            if (is_future_ready(resource.futureAsyncProcess)) { // Renamed member
                try {
                    resource.dataForMainThreadFinish = resource.futureAsyncProcess.get(); // Renamed members
                    resource.state = ManagedResource::State::PENDING_FINISH;
                    m_readyToFinishKeys.push_back(resource.resourceKey);
                    std::cout << "Info: Async completed for " << resource.resourceKey << ". Ready for finish." << std::endl;
                } catch (const std::exception& e) {
                    resource.state = ManagedResource::State::FAILED_ASYNC;
                    std::cerr << "Error during async processing (future.get) for " << resource.resourceKey << ": " << e.what() << std::endl;
                } catch (...) {
                    resource.state = ManagedResource::State::FAILED_ASYNC;
                    std::cerr << "Unknown error during async processing (future.get) for " << resource.resourceKey << std::endl;
                }
            }
        }
    }
}

void AsyncResourceManager::processMainThreadFinishers() { // Renamed class
    std::vector<std::string> keys_to_process_locally;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_readyToFinishKeys.empty()) {
            return;
        }
        keys_to_process_locally.swap(m_readyToFinishKeys);
    }

    std::vector<ResourceRequest> catch_up_requests; // Renamed struct

    for (const std::string& key : keys_to_process_locally) {
        IAsyncResourceGenerator* generator = nullptr; // Renamed interface
        std::any data_for_finish_call; // Renamed variable for clarity
        std::string stored_generator_id; 

        { 
            std::lock_guard<std::mutex> lock(m_mutex);
            auto it = m_managedResources.find(key);
            if (it == m_managedResources.end()) {
                std::cerr << "Warning: Resource " << key << " not found during main thread finish (was it removed?)." << std::endl;
                continue; 
            }
            ManagedResource& resource = it->second; // Type is now ManagedResource
            if (resource.state != ManagedResource::State::PENDING_FINISH) {
                std::cerr << "Warning: Resource " << key << " not in PENDING_FINISH state (current: " << static_cast<int>(resource.state) << ")." << std::endl;
                continue; 
            }
            
            if (resource.generatorId.empty()) {
                std::cerr << "Error: generatorId missing in ManagedResource for " << key << ". Cannot finish." << std::endl;
                resource.state = ManagedResource::State::FAILED_FINISH;
                continue;
            }
            stored_generator_id = resource.generatorId; // Use stored generatorId
            
            auto gen_it = m_generators.find(stored_generator_id);
            if (gen_it == m_generators.end()) {
                std::cerr << "Error: Generator ID '" << stored_generator_id << "' for " << key << " not found during finish." << std::endl;
                resource.state = ManagedResource::State::FAILED_FINISH;
                continue;
            }
            generator = gen_it->second.get();
            data_for_finish_call = std::move(resource.dataForMainThreadFinish); // Renamed member: preparedDataForFinisher -> dataForMainThreadFinish
        } 

        if (!generator) {
            // This case should be rare due to checks above, but as a safeguard:
            std::cerr << "Critical Error: Generator pointer null for " << key << " before calling FinishMainThread (after lock release)." << std::endl;
            // Attempt to re-lock and set state (can be complex, consider if necessary or log and move on)
            std::lock_guard<std::mutex> lock(m_mutex);
            if(m_managedResources.count(key)) {
                 m_managedResources[key].state = ManagedResource::State::FAILED_FINISH;
            }
            continue;
        }

        try {
            std::cout << "Info: Finishing main thread work for " << key << std::endl;
            std::any final_resource = generator->FinishMainThread(data_for_finish_call);
            
            std::lock_guard<std::mutex> lock(m_mutex);
            ManagedResource& resource = m_managedResources[key]; // Key must exist
            resource.completedResource = std::move(final_resource);
            resource.state = ManagedResource::State::COMPLETED;
            std::cout << "Info: Finishing complete for " << key << std::endl;

            if (resource.catchUpRequested) {
                std::cout << "Info: Catch-up requested for " << key << ". Scheduling re-request." << std::endl;
                ResourceRequest new_request; // Renamed struct
                new_request.resourceKey = key;
                new_request.generatorId = resource.generatorId; // Use stored generatorId
                new_request.inputData = resource.originalInputData_DEBUG_ONLY; // Use stored input data
                new_request.priority = 0; // Or stored priority if that was also added to ManagedResource
                catch_up_requests.push_back(new_request);
                resource.catchUpRequested = false; 
            }
        } catch (const std::exception& e) {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_managedResources.count(key)) {
                 m_managedResources[key].state = ManagedResource::State::FAILED_FINISH;
            }
            std::cerr << "Error during FinishMainThread for " << key << ": " << e.what() << std::endl;
        } catch (...) {
            std::lock_guard<std::mutex> lock(m_mutex);
             if (m_managedResources.count(key)) {
                m_managedResources[key].state = ManagedResource::State::FAILED_FINISH;
            }
            std::cerr << "Unknown error during FinishMainThread for " << key << std::endl;
        }
    }

    for (const auto& req : catch_up_requests) {
       std::cout << "Info: Issuing catch-up request for " << req.resourceKey << std::endl;
       requestResource(req); // This will lock m_mutex internally
    }
    // if (!catch_up_requests.empty()) { // This line is now part of the loop above.
    //     std::cout << "Info: " << catch_up_requests.size() << " catch-up requests to process." << std::endl;
    // }
}


std::any AsyncResourceManager::getResource(const std::string& resourceKey) const { // Renamed class
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_managedResources.find(resourceKey);
    if (it != m_managedResources.end() && it->second.state == ManagedResource::State::COMPLETED) { // Enum from ManagedResource
        return it->second.completedResource;
    }
    return std::any(); 
}

ManagedResource::State AsyncResourceManager::getResourceState(const std::string& resourceKey) const { // Renamed class and Enum from ManagedResource
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_managedResources.find(resourceKey);
    if (it != m_managedResources.end()) {
        return it->second.state;
    }
    return ManagedResource::State::IDLE; 
}

template bool AsyncResourceManager::is_future_ready<std::any>(const std::future<std::any>& future) const; // Renamed class
template bool AsyncResourceManager::is_future_ready<std::any>(const std::future<std::any>& future) const; // Renamed class

} // namespace async_resource_management
