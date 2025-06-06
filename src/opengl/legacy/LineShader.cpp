// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2024 The MMapper Authors

#include "LineShader.h" // Updated include
#include "GLRenderState.h" // Included for definition of GLRenderState::Uniforms
#include <glm/glm.hpp>     // For glm::mat4

namespace Legacy {

LineShader::~LineShader() = default; // Updated class name

void LineShader::virt_setUniforms(const glm::mat4 &mvp, const GLRenderState::Uniforms &uniforms) { // Updated class name
    // Set the Model-View-Projection matrix
    setMatrix("uMVP", mvp);

    // Set the modulation color. In the instanced line fragment shader,
    // this 'uColor' will modulate the per-instance color 'vColor'.
    // If no modulation is desired for typical line rendering, 'uniforms.color'
    // should effectively be white (1,1,1,1) when calling this for lines.
    setColor("uColor", uniforms.color);
}

} // namespace Legacy
