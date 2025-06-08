#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2023 The MMapper Authors

#include <glm/glm.hpp>
#include <optional>

// Forward declaration if glm::ivec2 is from a map-specific header, otherwise ensure glm is fully included.
// Assuming glm/glm.hpp is sufficient for glm::ivec2.

namespace MapCanvasUtils {
    // Projects world coordinates to screen coordinates (x, y)
    // The z component of the returned vec3 is typically depth or can be ignored if only 2D is needed.
    // For our specific use in getVisibleRoomIds, we only need the x and y for the screen bounds check.
    std::optional<glm::vec2> projectWorldToScreen(
        const glm::vec3& worldPoint,
        const glm::mat4& viewProjMatrix,
        const glm::ivec2& viewportSize // (width, height)
    );
}
