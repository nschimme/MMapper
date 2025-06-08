#pragma once

#include <vector>
#include <map>
#include <set>
#include <utility> // For std::pair
#include <array>   // For std::array
#include <glm/glm.hpp> // For glm::mat4, glm::vec4
#include <glm/vec3.hpp> // For glm::vec3 (if not covered by glm.hpp)
// RoomAreaHash is uint32_t, defined in roomid.h
#include "../map/roomid.h"

// Forward declarations for pointers used in VisibilityTaskParams
class MapData;
class World;

struct VisibilityTaskParams {
    int currentLayer = 0;
    float viewPortWidth = 0.0f;
    float viewPortHeight = 0.0f;
    glm::mat4 viewProjMatrix{1.f}; // Initialize to identity
    std::array<glm::vec4, 6> frustumPlanes;
    float vpWorldMinX = 0.0f;
    float vpWorldMaxX = 0.0f;
    float vpWorldMinY = 0.0f;
    float vpWorldMaxY = 0.0f;
    const MapData* mapDataPtr = nullptr;
    const World* worldPtr = nullptr;
    std::set<std::pair<int, RoomAreaHash>> existingValidMeshChunks;
    bool isHighPriorityRequest = false;
};

struct VisibilityTaskResult {
    std::map<int, std::set<RoomAreaHash>> visibleChunksCalculated;
    std::vector<std::pair<int, RoomAreaHash>> chunksToRequestGenerated;
    bool success = true;
    bool originatedFromHighPriorityRequest = false;
};
