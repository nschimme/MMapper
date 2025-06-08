// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2023 The MMapper Authors

#include "MapCanvasUtils.h"
#include <glm/gtc/matrix_transform.hpp> // For glm::any, glm::greaterThan, glm::abs, glm::clamp if not in glm.hpp
#include <cmath> // For std::abs

namespace MapCanvasUtils {

// Projects world coordinates to screen coordinates (x, y)
// worldPoint: The 3D point in world space to project.
// viewProjMatrix: The combined view-projection matrix.
// viewportSize: The dimensions (width, height) of the viewport in pixels.
// Returns std::optional<glm::vec2>. The x, y are screen coordinates.
// Screen coordinates are typically (0,0) at top-left or bottom-left depending on convention.
// This implementation matches the original MapCanvasViewport::project which implies
// (0,0) at top-left after y-flip, for a viewport.
std::optional<glm::vec2> projectWorldToScreen(
    const glm::vec3& worldPoint,
    const glm::mat4& viewProjMatrix,
    const glm::ivec2& viewportSize // (width, height)
) {
    const glm::vec4 tmp = viewProjMatrix * glm::vec4(worldPoint, 1.f);

    // This can happen if you set the layer height to the view distance
    // and then try to project a point on layer = 1, when the vertical
    // angle is 1, so the plane would pass through the camera.
    if (std::abs(tmp.w) < 1e-6f) {
        return std::nullopt;
    }
    const glm::vec3 ndc = glm::vec3{tmp} / tmp.w; // Normalized Device Coordinates: [-1, 1]^3 if clamped

    // Check if the point is outside the clipping volume in NDC space
    // A small epsilon is used to account for floating-point inaccuracies.
    const float epsilon = 1e-5f; // Match original logic
    if (ndc.x < -1.f - epsilon || ndc.x > 1.f + epsilon ||
        ndc.y < -1.f - epsilon || ndc.y > 1.f + epsilon ||
        ndc.z < -1.f - epsilon || ndc.z > 1.f + epsilon) { // Original also checks Z for full visibility
        // result is not visible on screen / outside near-far planes.
        return std::nullopt;
    }

    // Convert NDC to screen space [0, 1]^2 for x,y
    // (0,0) is typically bottom-left in NDC, but screen coords often top-left.
    // The formula (ndc * 0.5f + 0.5f) maps [-1,1] to [0,1].
    // Y is often flipped for screen coordinates (height - y_coord_in_bottom_left_system).
    const glm::vec2 screen_normalized = glm::vec2{ndc} * 0.5f + 0.5f;

    // Scale to viewport size.
    // Note: Original MapCanvasViewport::project uses viewport.offset, but for a general
    // utility, assuming viewport starts at (0,0) is common unless an offset is also passed.
    // The original adds viewport.offset at the end. If the viewport passed to this static
    // function is always the full window, then offset is 0. If it's a sub-viewport,
    // the caller of this static function would need to handle the offset if necessary.
    // For getVisibleRoomIds, we are checking against viewportWidth/Height which implies full viewport.
    glm::vec2 screen_coords = screen_normalized * glm::vec2{viewportSize};

    // The original MapCanvasViewport::project in MapCanvasData.cpp has:
    // const auto mouse = glm::vec2{screen} * glm::vec2{viewport.size} + glm::vec2{viewport.offset};
    // And `getViewport()` in MapCanvasData.h uses `m_sizeWidget.rect()`, which could have an offset.
    // However, `MapCanvas::getVisibleRoomIds` uses `width()` and `height()` which are full widget dimensions.
    // So, for `getVisibleRoomIds`, an offset of (0,0) is implicitly assumed for the viewport passed.

    return screen_coords;
}

} // namespace MapCanvasUtils
