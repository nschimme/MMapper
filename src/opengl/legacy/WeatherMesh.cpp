// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "WeatherMesh.h"

#include "../../display/Weather.h"
#include "../../global/random.h"
#include "Binders.h"
#include "Shaders.h"
#include "TFO.h"
#include "VAO.h"
#include "VBO.h"

#include <glm/glm.hpp>

namespace {
float get_random_float()
{
    return static_cast<float>(getRandom(1000000)) / 1000000.0f;
}

template<typename T>
T my_lerp(T a, T b, float t)
{
    return a + t * (b - a);
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

    auto emptyVao = m_functions.getSharedVaos().get(SharedVaoEnum::EmptyVao);
    if (!*emptyVao) {
        emptyVao->emplace(m_shared_functions);
    }
    VAOBinder vaoBinder(m_functions, emptyVao);

    // Texture unit 0 should be bound by RenderStateBinder if withTexture0 was used
    prog.setInt("uNoiseTex", 0);

    m_functions.glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

// WeatherTimeOfDayMesh

WeatherTimeOfDayMesh::WeatherTimeOfDayMesh(SharedFunctions shared_functions)
    : m_shared_functions(std::move(shared_functions))
    , m_functions(deref(m_shared_functions))
{}

WeatherTimeOfDayMesh::~WeatherTimeOfDayMesh() = default;

void WeatherTimeOfDayMesh::virt_render(const GLRenderState &renderState)
{
    auto &prog = deref(m_functions.getShaderPrograms().getTimeOfDayShader());
    auto binder = prog.bind();

    RenderStateBinder rsBinder(m_functions, m_functions.getTexLookup(), renderState);

    const glm::mat4 mvp = m_functions.getProjectionMatrix();
    prog.setUniforms(mvp, renderState.uniforms);

    auto vao = m_functions.getSharedVaos().get(SharedVaoEnum::WeatherTimeOfDay);
    if (!*vao) {
        vao->emplace(m_shared_functions);
    }
    VAOBinder vaoBinder(m_functions, vao);

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

    auto tf = m_functions.getSharedTfos().get(SharedTfEnum::WeatherSimulation);
    if (!*tf) {
        tf->emplace(m_shared_functions);
    }

    const auto buffer0 = SharedVboEnum::WeatherParticles0;
    const auto buffer1 = SharedVboEnum::WeatherParticles1;

    const size_t totalParticles = 1024;
    std::vector<float> initialData(totalParticles * 3, 0.0f);
    for (size_t i = 0; i < totalParticles; ++i) {
        initialData[i * 3 + 0] = get_random_float() * 28.0f - 14.0f;
        initialData[i * 3 + 1] = get_random_float() * 28.0f - 14.0f;
        initialData[i * 3 + 2] = get_random_float() * 1.0f;
    }

    auto vbo0 = m_functions.getSharedVbos().get(buffer0);
    auto vbo1 = m_functions.getSharedVbos().get(buffer1);

    if (!*vbo0)
        vbo0->emplace(m_shared_functions);
    if (!*vbo1)
        vbo1->emplace(m_shared_functions);

    {
        m_functions.glBindBuffer(GL_ARRAY_BUFFER, vbo0->get());
        m_functions.glBufferData(GL_ARRAY_BUFFER,
                                 static_cast<GLsizeiptr>(initialData.size() * sizeof(float)),
                                 initialData.data(),
                                 GL_STREAM_DRAW);
        m_functions.glBindBuffer(GL_ARRAY_BUFFER, 0);
    }
    {
        m_functions.glBindBuffer(GL_ARRAY_BUFFER, vbo1->get());
        m_functions.glBufferData(GL_ARRAY_BUFFER,
                                 static_cast<GLsizeiptr>(initialData.size() * sizeof(float)),
                                 initialData.data(),
                                 GL_STREAM_DRAW);
        m_functions.glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    // Initialize VAOs
    {
        auto vaoSim0 = m_functions.getSharedVaos().get(SharedVaoEnum::WeatherSimulation0);
        if (!*vaoSim0)
            vaoSim0->emplace(m_shared_functions);
        VAOBinder vaoBinder(m_functions, vaoSim0);
        m_functions.glBindBuffer(GL_ARRAY_BUFFER, vbo0->get());
        m_functions.enableAttrib(0, 2, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
        m_functions.enableAttrib(1,
                                 1,
                                 GL_FLOAT,
                                 GL_FALSE,
                                 3 * sizeof(float),
                                 reinterpret_cast<void *>(2 * sizeof(float)));
        m_functions.glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    {
        auto vaoSim1 = m_functions.getSharedVaos().get(SharedVaoEnum::WeatherSimulation1);
        if (!*vaoSim1)
            vaoSim1->emplace(m_shared_functions);
        VAOBinder vaoBinder(m_functions, vaoSim1);
        m_functions.glBindBuffer(GL_ARRAY_BUFFER, vbo1->get());
        m_functions.enableAttrib(0, 2, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
        m_functions.enableAttrib(1,
                                 1,
                                 GL_FLOAT,
                                 GL_FALSE,
                                 3 * sizeof(float),
                                 reinterpret_cast<void *>(2 * sizeof(float)));
        m_functions.glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    auto setupRenderVao = [&](SharedVaoEnum vaoEnum, SharedVboEnum vboEnum) {
        auto vao = m_functions.getSharedVaos().get(vaoEnum);
        if (!*vao)
            vao->emplace(m_shared_functions);
        auto vbo = m_functions.getSharedVbos().get(vboEnum);
        if (!*vbo)
            vbo->emplace(m_shared_functions);

        VAOBinder vaoBinder(m_functions, vao);
        m_functions.glBindBuffer(GL_ARRAY_BUFFER, vbo->get());
        m_functions.enableAttrib(0, 2, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
        m_functions.glVertexAttribDivisor(0, 1);
        m_functions.enableAttrib(1,
                                 1,
                                 GL_FLOAT,
                                 GL_FALSE,
                                 3 * sizeof(float),
                                 reinterpret_cast<void *>(2 * sizeof(float)));
        m_functions.glVertexAttribDivisor(1, 1);
        m_functions.glBindBuffer(GL_ARRAY_BUFFER, 0);
    };

    setupRenderVao(SharedVaoEnum::WeatherRender0, buffer0);
    setupRenderVao(SharedVaoEnum::WeatherRender1, buffer1);

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

    auto tf = m_functions.getSharedTfos().get(SharedTfEnum::WeatherSimulation);
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

WeatherParticleMesh::WeatherParticleMesh(SharedFunctions shared_functions, WeatherRenderer &renderer)
    : m_shared_functions(std::move(shared_functions))
    , m_functions(deref(m_shared_functions))
    , m_renderer(renderer)
{}

WeatherParticleMesh::~WeatherParticleMesh() = default;

void WeatherParticleMesh::virt_render(const GLRenderState &renderState)
{
    auto &state = m_renderer.getState();
    float t = std::clamp((state.animationTime - state.weatherTransitionStartTime) / 2.0f,
                         0.0f,
                         1.0f);
    float rain = my_lerp(state.rainIntensityStart, state.targetRainIntensity, t);
    float snow = my_lerp(state.snowIntensityStart, state.targetSnowIntensity, t);

    const float precipMax = std::max(rain, snow);
    if (precipMax <= 0.0f) {
        return;
    }

    auto &prog = deref(m_functions.getShaderPrograms().getParticleRenderShader());
    auto binder = prog.bind();

    RenderStateBinder rsBinder(m_functions, m_functions.getTexLookup(), renderState);

    const glm::mat4 mvp = m_functions.getProjectionMatrix();
    prog.setUniforms(mvp, renderState.uniforms);

    const GLsizei count = std::min(1024, static_cast<GLsizei>(std::ceil(precipMax * 1024.0f)));
    if (count > 0) {
        auto vaoEnum = (state.currentBuffer == 0) ? SharedVaoEnum::WeatherRender0
                                                  : SharedVaoEnum::WeatherRender1;
        auto vao = m_functions.getSharedVaos().get(vaoEnum);
        if (!*vao) {
            vao->emplace(m_shared_functions);
        }
        VAOBinder vaoBinder(m_functions, vao);
        m_functions.glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, count);
    }
}

} // namespace Legacy
