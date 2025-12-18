#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2023 The MMapper Authors

#include "AbstractShaderProgram.h"

namespace Legacy
{

class RoomShader : public AbstractShaderProgram
{
public:
    using AbstractShaderProgram::AbstractShaderProgram;

    void enableAttributes(OpenGL &gl);
    void setProjection(const glm::mat4 &projection);

private:
    void virt_setUniforms(const glm::mat4 &mvp, const GLRenderState::Uniforms &uniforms) final;
};

} // namespace modern
