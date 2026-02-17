// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "WeatherRenderables.h"

#include "../opengl/legacy/Binders.h"
#include "../opengl/legacy/Shaders.h"
#include "../opengl/legacy/TF.h"
#include "../opengl/legacy/VAO.h"
#include "../opengl/legacy/VBO.h"
#include "WeatherRenderer.h"
#include "../global/random.h"

#include <glm/glm.hpp>

namespace {
float get_random_float()
{
    return static_cast<float>(getRandom(1000000)) / 1000000.0f;
}
} // namespace

namespace Legacy {

// WeatherAtmosphereMesh

WeatherAtmosphereMesh::WeatherAtmosphereMesh(SharedFunctions shared_functions)
    : m_shared_functions(std::move(shared_functions))
    , m_functions(deref(m_shared_functions))
{}

WeatherAtmosphereMesh::~WeatherAtmosphereMesh() = default;

void WeatherAtmosphereMesh::virt_render(const GLRenderState &renderState)
{
    auto &prog = deref(m_functions.getShaderPrograms().getAtmosphereShader());
    auto binder = prog.bind();

    RenderStateBinder rsBinder(m_functions, m_functions.getTexLookup(), renderState);

    const glm::mat4 mvp = m_functions.getProjectionMatrix();
    prog.setUniforms(mvp, renderState.uniforms);
    prog.setMatrix("uInvViewProj", glm::inverse(mvp));

    auto emptyVao = m_functions.getSharedVaos().get(SharedVaoEnum::EmptyVao);
    if (!*emptyVao) {
        emptyVao->emplace(m_shared_functions);
    }
    VAOBinder vaoBinder(m_functions, emptyVao);

    // Texture unit 0 should be bound by RenderStateBinder if withTexture0 was used
    prog.setInt("uNoiseTex", 0);

    m_functions.glDrawArrays(GL_TRIANGLES, 0, 3);
}

// WeatherSimulationMesh

WeatherSimulationMesh::WeatherSimulationMesh(SharedFunctions shared_functions,
                                             WeatherRenderer &renderer)
    : m_shared_functions(std::move(shared_functions))
    , m_functions(deref(m_shared_functions))
    , m_renderer(renderer)
{}

WeatherSimulationMesh::~WeatherSimulationMesh() = default;

void WeatherSimulationMesh::init()
{
    auto &state = m_renderer.getState();
    if (state.initialized) {
        return;
    }

    auto tf = m_functions.getSharedTfs().get(SharedTfEnum::WeatherSimulation);
    if (!*tf) {
        tf->emplace(m_shared_functions);
    }

    const auto buffer0 = SharedVboEnum::WeatherParticles0;
    const auto buffer1 = SharedVboEnum::WeatherParticles1;

    const size_t totalParticles = 5120; // 4096 rain + 1024 snow
    std::vector<float> initialData(totalParticles * 3, 0.0f);
    for (size_t i = 0; i < totalParticles; ++i) {
        initialData[i * 3 + 0] = get_random_float() * 40.0f - 20.0f;
        initialData[i * 3 + 1] = get_random_float() * 40.0f - 20.0f;
        initialData[i * 3 + 2] = get_random_float() * 1.0f;
    }

    auto vbo0 = m_functions.getSharedVbos().get(buffer0);
    auto vbo1 = m_functions.getSharedVbos().get(buffer1);

    if (!*vbo0) vbo0->emplace(m_shared_functions);
    if (!*vbo1) vbo1->emplace(m_shared_functions);

    {
        m_functions.glBindBuffer(GL_ARRAY_BUFFER, vbo0->get());
        m_functions.glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(initialData.size() * sizeof(float)), initialData.data(), GL_STREAM_DRAW);
        m_functions.glBindBuffer(GL_ARRAY_BUFFER, 0);
    }
    {
        m_functions.glBindBuffer(GL_ARRAY_BUFFER, vbo1->get());
        m_functions.glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(initialData.size() * sizeof(float)), initialData.data(), GL_STREAM_DRAW);
        m_functions.glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    // Initialize VAOs
    {
        auto vaoSim0 = m_functions.getSharedVaos().get(SharedVaoEnum::WeatherSimulation0);
        if (!*vaoSim0) vaoSim0->emplace(m_shared_functions);
        VAOBinder vaoBinder(m_functions, vaoSim0);
        m_functions.glBindBuffer(GL_ARRAY_BUFFER, vbo0->get());
        m_functions.enableAttrib(0, 2, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
        m_functions.enableAttrib(1, 1, GL_FLOAT, GL_FALSE, 3 * sizeof(float), reinterpret_cast<void *>(2 * sizeof(float)));
        m_functions.glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    {
        auto vaoSim1 = m_functions.getSharedVaos().get(SharedVaoEnum::WeatherSimulation1);
        if (!*vaoSim1) vaoSim1->emplace(m_shared_functions);
        VAOBinder vaoBinder(m_functions, vaoSim1);
        m_functions.glBindBuffer(GL_ARRAY_BUFFER, vbo1->get());
        m_functions.enableAttrib(0, 2, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
        m_functions.enableAttrib(1, 1, GL_FLOAT, GL_FALSE, 3 * sizeof(float), reinterpret_cast<void *>(2 * sizeof(float)));
        m_functions.glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    auto setupRenderVao = [&](SharedVaoEnum vaoEnum, SharedVboEnum vboEnum, size_t offset) {
        auto vao = m_functions.getSharedVaos().get(vaoEnum);
        if (!*vao) vao->emplace(m_shared_functions);
        auto vbo = m_functions.getSharedVbos().get(vboEnum);
        if (!*vbo) vbo->emplace(m_shared_functions);

        VAOBinder vaoBinder(m_functions, vao);
        m_functions.glBindBuffer(GL_ARRAY_BUFFER, vbo->get());
        m_functions.enableAttrib(0, 2, GL_FLOAT, GL_FALSE, 3 * sizeof(float), reinterpret_cast<void *>(offset));
        m_functions.glVertexAttribDivisor(0, 1);
        m_functions.enableAttrib(1, 1, GL_FLOAT, GL_FALSE, 3 * sizeof(float), reinterpret_cast<void *>(offset + 2 * sizeof(float)));
        m_functions.glVertexAttribDivisor(1, 1);
        m_functions.glBindBuffer(GL_ARRAY_BUFFER, 0);
    };

    setupRenderVao(SharedVaoEnum::WeatherRenderRain0, buffer0, 0);
    setupRenderVao(SharedVaoEnum::WeatherRenderRain1, buffer1, 0);
    setupRenderVao(SharedVaoEnum::WeatherRenderSnow0, buffer0, 4096 * 3 * sizeof(float));
    setupRenderVao(SharedVaoEnum::WeatherRenderSnow1, buffer1, 4096 * 3 * sizeof(float));

    state.numParticles = static_cast<uint32_t>(totalParticles);
    state.initialized = true;
}

void WeatherSimulationMesh::virt_render(const GLRenderState &renderState)
{
    init();
    auto &state = m_renderer.getState();

    auto &prog = deref(m_functions.getShaderPrograms().getParticleSimulationShader());
    auto binder = prog.bind();

    const glm::mat4 mvp = m_functions.getProjectionMatrix();
    prog.setUniforms(mvp, renderState.uniforms);

    const auto bufferOut = (state.currentBuffer == 0) ? SharedVboEnum::WeatherParticles1
                                                      : SharedVboEnum::WeatherParticles0;
    const auto vboOut = m_functions.getSharedVbos().get(bufferOut);
    if (!*vboOut) {
        vboOut->emplace(m_shared_functions);
    }

    auto vaoEnum = (state.currentBuffer == 0) ? SharedVaoEnum::WeatherSimulation0
                                               : SharedVaoEnum::WeatherSimulation1;
    auto vao = m_functions.getSharedVaos().get(vaoEnum);
    if (!*vao) {
        vao->emplace(m_shared_functions);
    }
    VAOBinder vaoBinder(m_functions, vao);

    auto tf = m_functions.getSharedTfs().get(SharedTfEnum::WeatherSimulation);
    if (!*tf) {
        tf->emplace(m_shared_functions);
    }

    m_functions.glEnable(GL_RASTERIZER_DISCARD);
    m_functions.glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, tf->get());
    m_functions.glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, vboOut->get());
    {
        TransformFeedbackBinder tfBinder(m_functions, tf, GL_POINTS);
        m_functions.glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(state.numParticles));
    }
    m_functions.glDisable(GL_RASTERIZER_DISCARD);

    // Swap buffers for the next pass
    state.currentBuffer = 1 - state.currentBuffer;
}

// WeatherParticleMesh

WeatherParticleMesh::WeatherParticleMesh(SharedFunctions shared_functions,
                                         WeatherRenderer &renderer)
    : m_shared_functions(std::move(shared_functions))
    , m_functions(deref(m_shared_functions))
    , m_renderer(renderer)
{}

WeatherParticleMesh::~WeatherParticleMesh() = default;

void WeatherParticleMesh::virt_render(const GLRenderState &renderState)
{
    auto &state = m_renderer.getState();
    const float rainMax = std::max(state.rainIntensityStart, state.targetRainIntensity);
    const float snowMax = std::max(state.snowIntensityStart, state.targetSnowIntensity);

    auto &prog = deref(m_functions.getShaderPrograms().getParticleRenderShader());
    auto binder = prog.bind();

    RenderStateBinder rsBinder(m_functions, m_functions.getTexLookup(), renderState);

    const glm::mat4 mvp = m_functions.getProjectionMatrix();
    prog.setUniforms(mvp, renderState.uniforms);

    // Rain
    const GLsizei rainCount = std::min(4096, static_cast<GLsizei>(std::ceil(rainMax * 2048.0f)));
    if (rainCount > 0) {
        auto vaoRainEnum = (state.currentBuffer == 0) ? SharedVaoEnum::WeatherRenderRain0
                                                       : SharedVaoEnum::WeatherRenderRain1;
        auto vao = m_functions.getSharedVaos().get(vaoRainEnum);
        if (!*vao) {
            vao->emplace(m_shared_functions);
        }
        VAOBinder vaoBinder(m_functions, vao);
        prog.setFloat("uType", 0.0f);
        prog.setInt("uInstanceOffset", 0);
        m_functions.glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, rainCount);
    }

    // Snow
    const GLsizei snowCount = std::min(1024, static_cast<GLsizei>(std::ceil(snowMax * 1024.0f)));
    if (snowCount > 0) {
        auto vaoSnowEnum = (state.currentBuffer == 0) ? SharedVaoEnum::WeatherRenderSnow0
                                                       : SharedVaoEnum::WeatherRenderSnow1;
        auto vao = m_functions.getSharedVaos().get(vaoSnowEnum);
        if (!*vao) {
            vao->emplace(m_shared_functions);
        }
        VAOBinder vaoBinder(m_functions, vao);
        prog.setFloat("uType", 1.0f);
        prog.setInt("uInstanceOffset", 4096);
        m_functions.glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, snowCount);
    }
}

} // namespace Legacy
