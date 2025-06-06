#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "AbstractShaderProgram.h"
#include "Legacy.h" // Added for Functions& member in ShaderPrograms

#include <memory>

namespace Legacy {

struct NODISCARD AColorPlainShader : public AbstractShaderProgram
{
public:
    using AbstractShaderProgram::AbstractShaderProgram;

    virtual ~AColorPlainShader() override;

protected:
    void virt_setUniforms(const glm::mat4 &mvp, const GLRenderState::Uniforms &uniforms) override
    {
        setColor("uColor", uniforms.color);
        setMatrix("uMVP", mvp);
    }
};

struct NODISCARD AColorThickLineShader final : public AColorPlainShader
{
public:
    AColorThickLineShader(std::string dirName, SharedFunctions functions, Program program)
        : AColorPlainShader(std::move(dirName), std::move(functions), std::move(program)) {}
public: // Destructor should be public
    ~AColorThickLineShader() override final;
protected: // virt_setUniforms changed to protected
    void virt_setUniforms(const glm::mat4 &mvp, const GLRenderState::Uniforms &uniforms) override final {
        AColorPlainShader::virt_setUniforms(mvp, uniforms); // Call base
        setFloat("uLineWidth", uniforms.lineWidth);
    }
};

// Moved UColorPlainShader before UColorThickLineShader
struct NODISCARD UColorPlainShader : public AbstractShaderProgram
{
public:
    using AbstractShaderProgram::AbstractShaderProgram;

    virtual ~UColorPlainShader() override;

protected:
    void virt_setUniforms(const glm::mat4 &mvp, const GLRenderState::Uniforms &uniforms) override
    {
        setColor("uColor", uniforms.color);
        setMatrix("uMVP", mvp);
    }
};

struct NODISCARD UColorThickLineShader final : public UColorPlainShader
{
public:
    UColorThickLineShader(std::string dirName, SharedFunctions functions, Program program)
        : UColorPlainShader(std::move(dirName), std::move(functions), std::move(program)) {}
public: // Destructor should be public
    ~UColorThickLineShader() override final;
protected: // virt_setUniforms changed to protected
    void virt_setUniforms(const glm::mat4 &mvp, const GLRenderState::Uniforms &uniforms) override final {
        UColorPlainShader::virt_setUniforms(mvp, uniforms); // Call base
        setFloat("uLineWidth", uniforms.lineWidth);
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
    std::shared_ptr<AColorThickLineShader> aColorThickLineShader;
    std::shared_ptr<UColorThickLineShader> uColorThickLineShader;

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
        aColorThickLineShader.reset();
        uColorThickLineShader.reset();
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
    NODISCARD const std::shared_ptr<AColorThickLineShader> &getPlainAColorThickLineShader();
    NODISCARD const std::shared_ptr<UColorThickLineShader> &getPlainUColorThickLineShader();
};

} // namespace Legacy
