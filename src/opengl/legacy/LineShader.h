#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2024 The MMapper Authors

// Ensure this guard is unique, e.g., LEGACY_LINE_SHADER_H_
#ifndef LEGACY_LINE_SHADER_H_
#define LEGACY_LINE_SHADER_H_

#include "AbstractShaderProgram.h"
#include "GLRenderState.h" // For GLRenderState::Uniforms

// Forward declaration if GLM is not included via AbstractShaderProgram.h or Legacy.h
namespace glm {
    struct mat4;
}

namespace Legacy {

struct NODISCARD LineShader final : public AbstractShaderProgram {
public:
    // Inherit constructors from AbstractShaderProgram
    // The first argument to the base constructor would be the directory name
    // for the shader files, e.g., "line" (updated from "instanced_thick_line")
    using AbstractShaderProgram::AbstractShaderProgram;

    ~LineShader() final;

private:
    void virt_setUniforms(const glm::mat4 &mvp, const GLRenderState::Uniforms &uniforms) final;
};

} // namespace Legacy

#endif // LEGACY_LINE_SHADER_H_
