#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "../OpenGL.h"
#include "../OpenGLTypes.h"
#include "Binders.h"
#include "Legacy.h"
#include "Shaders.h"
#include "TFO.h"
#include "VAO.h"
#include "VBO.h"

namespace Legacy {

class WeatherRenderer;

/**
 * @brief Base class for meshes that draw a full-screen triangle using gl_VertexID.
 */
template<typename ProgramType_>
class NODISCARD WeatherFullScreenMesh : public IRenderable
{
protected:
    const SharedFunctions m_shared_functions;
    Functions &m_functions;
    const std::shared_ptr<ProgramType_> m_shared_program;
    ProgramType_ &m_program;
    VAO m_vao;
    const GLenum m_mode;
    const GLsizei m_numVerts;

public:
    explicit WeatherFullScreenMesh(SharedFunctions sharedFunctions,
                                   std::shared_ptr<ProgramType_> sharedProgram,
                                   GLenum mode = GL_TRIANGLES,
                                   GLsizei numVerts = 3)
        : m_shared_functions{std::move(sharedFunctions)}
        , m_functions{deref(m_shared_functions)}
        , m_shared_program{std::move(sharedProgram)}
        , m_program{deref(m_shared_program)}
        , m_mode(mode)
        , m_numVerts(numVerts)
    {
        m_vao.emplace(m_shared_functions);
    }

    ~WeatherFullScreenMesh() override { reset(); }

protected:
    void virt_clear() final {}
    void virt_reset() final { m_vao.reset(); }
    NODISCARD bool virt_isEmpty() const final { return !m_vao; }

    void virt_render(const GLRenderState &renderState) override
    {
        if (isEmpty()) {
            return;
        }

        auto binder = m_program.bind();
        const glm::mat4 mvp = m_functions.getProjectionMatrix();
        m_program.setUniforms(mvp, renderState.uniforms);

        RenderStateBinder rsBinder(m_functions, m_functions.getTexLookup(), renderState);

        m_functions.glBindVertexArray(m_vao);
        m_functions.glDrawArrays(m_mode, 0, m_numVerts);
        m_functions.glBindVertexArray(0);
    }
};

class NODISCARD WeatherAtmosphereMesh final : public WeatherFullScreenMesh<AtmosphereShader>
{
private:
    SharedMMTexture m_noiseTexture;

public:
    explicit WeatherAtmosphereMesh(SharedFunctions sharedFunctions,
                                   std::shared_ptr<AtmosphereShader> program,
                                   SharedMMTexture noiseTexture)
        : WeatherFullScreenMesh(std::move(sharedFunctions), std::move(program), GL_TRIANGLE_STRIP, 4)
        , m_noiseTexture(std::move(noiseTexture))
    {}

private:
    NODISCARD bool virt_modifiesRenderState() const override { return true; }
    NODISCARD GLRenderState virt_modifyRenderState(const GLRenderState &input) const override;

public:
    void virt_render(const GLRenderState &renderState) override;
};

class NODISCARD WeatherTimeOfDayMesh final : public WeatherFullScreenMesh<TimeOfDayShader>
{
public:
    using WeatherFullScreenMesh::WeatherFullScreenMesh;
};

class NODISCARD WeatherSimulationMesh final : public IRenderable
{
private:
    const SharedFunctions m_shared_functions;
    Functions &m_functions;
    const std::shared_ptr<ParticleSimulationShader> m_program;

    TFO m_tfo;
    VBO m_vbos[2];
    VAO m_vaos[2]; // VAOs for simulation (reading from VBOs)

    uint32_t m_currentBuffer = 0;
    uint32_t m_numParticles = 1024;
    bool m_initialized = false;

public:
    explicit WeatherSimulationMesh(SharedFunctions shared_functions,
                                   std::shared_ptr<ParticleSimulationShader> program);
    ~WeatherSimulationMesh() override;

private:
    void init();
    void virt_clear() override {}
    void virt_reset() override;
    NODISCARD bool virt_isEmpty() const override { return !m_initialized; }
    void virt_render(const GLRenderState &renderState) override;

public:
    NODISCARD uint32_t getCurrentBuffer() const { return m_currentBuffer; }
    NODISCARD uint32_t getNumParticles() const { return m_numParticles; }
    NODISCARD const VBO &getParticleVbo(uint32_t index) const { return m_vbos[index]; }
};

class NODISCARD WeatherParticleMesh final : public IRenderable
{
private:
    const SharedFunctions m_shared_functions;
    Functions &m_functions;
    const std::shared_ptr<ParticleRenderShader> m_program;
    const WeatherSimulationMesh &m_simulation;

    VAO m_vaos[2]; // VAOs for rendering (reading from VBOs with instancing)

public:
    explicit WeatherParticleMesh(SharedFunctions shared_functions,
                                 std::shared_ptr<ParticleRenderShader> program,
                                 const WeatherSimulationMesh &simulation);
    ~WeatherParticleMesh() override;

public:
    void render(const GLRenderState &renderState, float rain, float snow);

private:
    void init();
    void virt_clear() override {}
    void virt_reset() override;
    NODISCARD bool virt_isEmpty() const override { return false; }
    void virt_render(const GLRenderState &renderState) override;
};

} // namespace Legacy
