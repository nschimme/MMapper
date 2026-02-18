// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "WeatherMeshes.h"

#include "../../display/Textures.h"
#include "../../global/random.h"
#include "Binders.h"

namespace Legacy {

GLRenderState WeatherAtmosphereMesh::virt_modifyRenderState(const GLRenderState &input) const
{
    return input.withTexture0(m_noiseTexture->getId());
}

void WeatherAtmosphereMesh::virt_render(const GLRenderState &renderState)
{
    WeatherFullScreenMesh::virt_render(renderState);
}

WeatherSimulationMesh::WeatherSimulationMesh(SharedFunctions shared_functions,
                                             std::shared_ptr<ParticleSimulationShader> program)
    : m_shared_functions(std::move(shared_functions))
    , m_functions(deref(m_shared_functions))
    , m_program(std::move(program))
{
    m_tfo.emplace(m_shared_functions);
    for (int i = 0; i < 2; ++i) {
        m_vbos[i].emplace(m_shared_functions);
        m_vaos[i].emplace(m_shared_functions);
    }
}

WeatherSimulationMesh::~WeatherSimulationMesh() = default;

void WeatherSimulationMesh::init()
{
    if (m_initialized) {
        return;
    }

    auto get_random_float = []() { return static_cast<float>(getRandom(1000000)) / 1000000.0f; };

    std::vector<float> initialData(m_numParticles * 3, 0.0f);
    for (size_t i = 0; i < m_numParticles; ++i) {
        initialData[i * 3 + 0] = get_random_float() * 28.0f - 14.0f;
        initialData[i * 3 + 1] = get_random_float() * 28.0f - 14.0f;
        initialData[i * 3 + 2] = get_random_float() * 1.0f;
    }

    for (int i = 0; i < 2; ++i) {
        m_functions.glBindBuffer(GL_ARRAY_BUFFER, m_vbos[i].get());
        m_functions.glBufferData(GL_ARRAY_BUFFER,
                                 static_cast<GLsizeiptr>(initialData.size() * sizeof(float)),
                                 initialData.data(),
                                 GL_STREAM_DRAW);

        m_functions.glBindVertexArray(m_vaos[i].get());
        m_functions.enableAttrib(0, 2, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
        m_functions.enableAttrib(1,
                                 1,
                                 GL_FLOAT,
                                 GL_FALSE,
                                 3 * sizeof(float),
                                 reinterpret_cast<void *>(2 * sizeof(float)));
        m_functions.glBindVertexArray(0);
        m_functions.glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    m_initialized = true;
}

void WeatherSimulationMesh::virt_reset()
{
    m_tfo.reset();
    for (int i = 0; i < 2; ++i) {
        m_vbos[i].reset();
        m_vaos[i].reset();
    }
    m_initialized = false;
}

void WeatherSimulationMesh::virt_render(const GLRenderState &renderState)
{
    init();

    auto binder = m_program->bind();
    const glm::mat4 mvp = m_functions.getProjectionMatrix();
    m_program->setUniforms(mvp, renderState.uniforms);

    const uint32_t bufferOut = 1 - m_currentBuffer;

    m_functions.glBindVertexArray(m_vaos[m_currentBuffer]);
    m_functions.glEnable(GL_RASTERIZER_DISCARD);
    m_functions.glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, m_tfo.get());
    m_functions.glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, m_vbos[bufferOut].get());

    {
        // Direct GL calls since we don't have a SharedTfo
        m_functions.glBeginTransformFeedback(GL_POINTS);
        m_functions.glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(m_numParticles));
        m_functions.glEndTransformFeedback();
    }
    m_functions.glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, 0);
    m_functions.glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, 0);
    m_functions.glDisable(GL_RASTERIZER_DISCARD);
    m_functions.glBindVertexArray(0);

    m_currentBuffer = bufferOut;
}

WeatherParticleMesh::WeatherParticleMesh(SharedFunctions shared_functions,
                                         std::shared_ptr<ParticleRenderShader> program,
                                         const WeatherSimulationMesh &simulation)
    : m_shared_functions(std::move(shared_functions))
    , m_functions(deref(m_shared_functions))
    , m_program(std::move(program))
    , m_simulation(simulation)
{
    for (int i = 0; i < 2; ++i) {
        m_vaos[i].emplace(m_shared_functions);
    }
}

WeatherParticleMesh::~WeatherParticleMesh() = default;

void WeatherParticleMesh::init()
{
    // Attributes are re-bound every frame to ensure they point to correct VBOs from simulation
}

void WeatherParticleMesh::virt_reset()
{
    for (int i = 0; i < 2; ++i) {
        m_vaos[i].reset();
    }
}

void WeatherParticleMesh::render(const GLRenderState &renderState, float rain, float snow)
{
    const float precipMax = std::max(rain, snow);
    const GLsizei count = std::min(1024, static_cast<GLsizei>(std::ceil(precipMax * 1024.0f)));
    if (count <= 0) {
        return;
    }

    auto binder = m_program->bind();
    const glm::mat4 mvp = m_functions.getProjectionMatrix();
    m_program->setUniforms(mvp, renderState.uniforms);

    RenderStateBinder rsBinder(m_functions, m_functions.getTexLookup(), renderState);

    const uint32_t bufferIdx = m_simulation.getCurrentBuffer();

    m_functions.glBindVertexArray(m_vaos[bufferIdx]);

    m_functions.glBindBuffer(GL_ARRAY_BUFFER, m_simulation.getParticleVbo(bufferIdx));
    m_functions.enableAttrib(0, 2, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
    m_functions.glVertexAttribDivisor(0, 1);
    m_functions.enableAttrib(1,
                             1,
                             GL_FLOAT,
                             GL_FALSE,
                             3 * sizeof(float),
                             reinterpret_cast<void *>(2 * sizeof(float)));
    m_functions.glVertexAttribDivisor(1, 1);

    m_functions.glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, count);

    m_functions.glBindVertexArray(0);
    m_functions.glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void WeatherParticleMesh::virt_render(const GLRenderState & /*renderState*/)
{
    assert(false); // Use the other render overload
}

} // namespace Legacy
