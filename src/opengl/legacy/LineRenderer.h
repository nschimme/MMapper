#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2024 The MMapper Authors

// Ensure this guard is unique, e.g., LEGACY_LINE_RENDERER_H_
#ifndef LEGACY_LINE_RENDERER_H_
#define LEGACY_LINE_RENDERER_H_

#include "Legacy.h" // For SharedFunctions, Functions, GLRenderState
#include "VBO.h"    // For VBO class
#include "LineShader.h" // Updated include: The shader this renderer will use

#include <glm/glm.hpp>
#include <vector>
#include <memory> // For std::shared_ptr

// QOpenGLExtraFunctions forward declaration removed as it's no longer a direct member/parameter.
// Full include will be in .cpp file if needed there.

namespace Legacy {

// Data structure for each instance of a thick line segment
// This remains the same as it's specific to line rendering logic, not the shader name
struct LineInstanceData {
    glm::vec2 startPoint;
    glm::vec2 endPoint;
    float thickness;
    glm::vec4 color; // Using glm::vec4 for color matching shader
};

class NODISCARD LineRenderer { // Updated class name
private:
    SharedFunctions m_shared_functions;
    Functions& m_functions; // Reference to Functions object
    std::shared_ptr<LineShader> m_shader; // Updated shader type
    // QOpenGLExtraFunctions* m_extraFunctions; // REMOVED

    VBO m_baseQuadVBO;     // VBO for the 4 vertices of the base quad
    VBO m_instanceDataVBO; // VBO for per-instance data
    GLuint m_vao = 0;      // Vertex Array Object

    GLsizei m_numInstances = 0;

    // Attribute locations (to match GLSL layout locations)
    // These could be queried, but using direct layout locations is common.
    static constexpr GLuint LOC_BASE_VERTEX_POS = 0;
    static constexpr GLuint LOC_INSTANCE_START_POINT = 1;
    static constexpr GLuint LOC_INSTANCE_END_POINT = 2;
    static constexpr GLuint LOC_INSTANCE_THICKNESS = 3;
    static constexpr GLuint LOC_INSTANCE_COLOR = 4;

public:
    LineRenderer(SharedFunctions sharedFunctions,
                 std::shared_ptr<LineShader> shader); // Constructor signature changed
    ~LineRenderer();

    // Delete copy and move operations to prevent accidental copying of OpenGL resources
    LineRenderer(const LineRenderer&) = delete;
    LineRenderer& operator=(const LineRenderer&) = delete;
    LineRenderer(LineRenderer&&) = delete;
    LineRenderer& operator=(LineRenderer&&) = delete;

    void setup(); // Creates VAO, VBOs, sets up attributes
    void updateInstanceData(const std::vector<LineInstanceData>& instanceData);
    void render(const glm::mat4& mvp, const GLRenderState::Uniforms& uniforms);

private:
    void setupVAO(); // Internal VAO setup logic
};

} // namespace Legacy

#endif // LEGACY_LINE_RENDERER_H_
