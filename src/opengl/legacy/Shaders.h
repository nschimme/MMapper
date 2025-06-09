#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "AbstractShaderProgram.h"

#include <memory>

namespace Legacy {

class InstancedArrayIconProgram; // Forward declaration

struct NODISCARD AColorPlainShader final : public AbstractShaderProgram
{
public:
    using AbstractShaderProgram::AbstractShaderProgram;

    ~AColorPlainShader() final;

private:
    void virt_setUniforms(const glm::mat4 &mvp, const GLRenderState::Uniforms &uniforms) final
    {
        setColor("uColor", uniforms.color);
        setMatrix("uMVP", mvp);
    }
};

struct NODISCARD UColorPlainShader final : public AbstractShaderProgram
{
public:
    using AbstractShaderProgram::AbstractShaderProgram;

    ~UColorPlainShader() final;

private:
    void virt_setUniforms(const glm::mat4 &mvp, const GLRenderState::Uniforms &uniforms) final
    {
        setColor("uColor", uniforms.color);
        setMatrix("uMVP", mvp);
    }
};

struct NODISCARD AColorTexturedShader final : public AbstractShaderProgram
{
public:
    using AbstractShaderProgram::AbstractShaderProgram;

    ~AColorTexturedShader() final;

private:
    void virt_setUniforms(const glm::mat4 &mvp, const GLRenderState::Uniforms &uniforms) final
    {
        assert(uniforms.textures[0] != INVALID_MM_TEXTURE_ID);

        setColor("uColor", uniforms.color);
        setMatrix("uMVP", mvp);
        setTexture("uTexture", 0);
    }
};

struct NODISCARD UColorTexturedShader final : public AbstractShaderProgram
{
public:
    using AbstractShaderProgram::AbstractShaderProgram;

    ~UColorTexturedShader() final;

private:
    void virt_setUniforms(const glm::mat4 &mvp, const GLRenderState::Uniforms &uniforms) final
    {
        assert(uniforms.textures[0] != INVALID_MM_TEXTURE_ID);

        setColor("uColor", uniforms.color);
        setMatrix("uMVP", mvp);
        setTexture("uTexture", 0);
    }
};

struct NODISCARD FontShader final : public AbstractShaderProgram
{
private:
    using Base = AbstractShaderProgram;

public:
    using Base::AbstractShaderProgram;

    ~FontShader() final;

private:
    void virt_setUniforms(const glm::mat4 &mvp, const GLRenderState::Uniforms &uniforms) final
    {
        assert(uniforms.textures[0] != INVALID_MM_TEXTURE_ID);
        auto functions = Base::m_functions.lock();

        setMatrix("uMVP3D", mvp);
        setTexture("uFontTexture", 0);
        setViewport("uPhysViewport", deref(functions).getPhysicalViewport());
    }
};

struct NODISCARD PointShader final : public AbstractShaderProgram
{
public:
    using AbstractShaderProgram::AbstractShaderProgram;

    ~PointShader() final;

private:
    void virt_setUniforms(const glm::mat4 &mvp, const GLRenderState::Uniforms &uniforms) final
    {
        setColor("uColor", uniforms.color);
        setMatrix("uMVP", mvp);
    }
};

// New Shader Program for Texture Arrays (behaves like UColorTexturedShader but for arrays)
struct NODISCARD TexturedArrayProgram final : public AbstractShaderProgram
{
public:
    using AbstractShaderProgram::AbstractShaderProgram;
    ~TexturedArrayProgram() final;

private:
    void virt_setUniforms(const glm::mat4 &mvp, const GLRenderState::Uniforms &uniforms) final;
};

// New program for instanced icon rendering
class NODISCARD InstancedArrayIconProgram final : public AbstractShaderProgram {
public:
    using AbstractShaderProgram::AbstractShaderProgram;
    ~InstancedArrayIconProgram() final;

    // Factory method
    NODISCARD static std::shared_ptr<InstancedArrayIconProgram> create(
        Functions &functions,
        const std::string &vert_resource_path, // e.g., ":/shaders/legacy/instanced_icons/vert.glsl"
        const std::string &frag_resource_path  // e.g., ":/shaders/legacy/instanced_icons/frag.glsl"
    );

private:
    // Uniform locations (cache them after linking)
    GLint m_u_projection_view_matrix_loc = INVALID_UNIFORM_LOCATION;
    GLint m_u_icon_base_size_loc = INVALID_UNIFORM_LOCATION;
    GLint m_u_tex_array_sampler_loc = INVALID_UNIFORM_LOCATION;

    void cacheUniformLocations(); // Helper to call after linking
    void virt_setUniforms(const glm::mat4 &mvp, const GLRenderState::Uniforms &uniforms) final;
    // Note: bindAttributes is not a virtual method in AbstractShaderProgram.
    // It's usually a static helper or called during the 'create' process.
    // For this class, it will be handled by the preLinkCallback in its static create method.
public:
    // Specific setters if needed, beyond virt_setUniforms
    void setProjectionViewMatrix(const glm::mat4& matrix);
    void setIconBaseSize(float size);
    void setTextureSampler(int texture_unit);

};


/* owned by Functions */
struct NODISCARD ShaderPrograms final
{
private:
    Functions &m_functions;
    std::shared_ptr<AColorPlainShader> aColorShader;
    std::shared_ptr<UColorPlainShader> uColorShader;
    std::shared_ptr<AColorTexturedShader> aTexturedShader;
    std::shared_ptr<UColorTexturedShader> uTexturedShader;
    std::shared_ptr<FontShader> font;
    std::shared_ptr<PointShader> point;
    std::shared_ptr<TexturedArrayProgram> m_texturedArrayProgram;
    std::shared_ptr<InstancedArrayIconProgram> m_instancedArrayIconProgram; // New program for instanced icons

public:
    explicit ShaderPrograms(Functions &functions)
        : m_functions{functions}
    {}

    DELETE_CTORS_AND_ASSIGN_OPS(ShaderPrograms);

private:
    NODISCARD Functions &getFunctions() { return m_functions; }

public:
    void resetAll()
    {
        aColorShader.reset();
        uColorShader.reset();
        aTexturedShader.reset();
        uTexturedShader.reset();
        font.reset();
        point.reset();
        m_texturedArrayProgram.reset(); // New program
    }

public:
    // attribute color (aka "Colored")
    NODISCARD const std::shared_ptr<AColorPlainShader> &getPlainAColorShader();
    // uniform color (aka "Plain")
    NODISCARD const std::shared_ptr<UColorPlainShader> &getPlainUColorShader();
    // attribute color + textured (aka "ColoredTextured")
    NODISCARD const std::shared_ptr<AColorTexturedShader> &getTexturedAColorShader();
    // uniform color + textured (aka "Textured")
    NODISCARD const std::shared_ptr<UColorTexturedShader> &getTexturedUColorShader();
    NODISCARD const std::shared_ptr<FontShader> &getFontShader();
    NODISCARD const std::shared_ptr<PointShader> &getPointShader();
    NODISCARD const std::shared_ptr<TexturedArrayProgram> &getTexturedArrayProgram();
    NODISCARD std::shared_ptr<InstancedArrayIconProgram> getInstancedArrayIconProgram(); // New getter
};

} // namespace Legacy
