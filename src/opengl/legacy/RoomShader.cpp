// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2023 The MMapper Authors

#include "RoomShader.h"
#include "../OpenGL.h"
#include "../modern/RoomInstanceData.h"
#include "Shaders.h"
#include "ShaderUtils.h"

namespace Legacy
{

void RoomShader::setProjection(const glm::mat4 &projection)
{
    setUniformMatrix4fv(getUniformLocation("u_projection"), 1, GL_FALSE, glm::value_ptr(projection));
}

void RoomShader::virt_setUniforms(const glm::mat4 &mvp, const GLRenderState::Uniforms &uniforms)
{
    // Not implemented yet
}

void RoomShader::enableAttributes(OpenGL &gl)
{
    const auto stride = sizeof(modern::RoomInstanceData);
    gl.getFunctions().glEnableVertexAttribArray(0);
    gl.getFunctions().glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<const void *>(offsetof(modern::RoomInstanceData, position)));
    gl.getFunctions().glVertexAttribDivisor(0, 1);

    gl.getFunctions().glEnableVertexAttribArray(1);
    gl.getFunctions().glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<const void *>(offsetof(modern::RoomInstanceData, tex_coord)));
    gl.getFunctions().glVertexAttribDivisor(1, 1);

    gl.getFunctions().glEnableVertexAttribArray(2);
    gl.getFunctions().glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<const void *>(offsetof(modern::RoomInstanceData, color)));
    gl.getFunctions().glVertexAttribDivisor(2, 1);
}

} // namespace Legacy
