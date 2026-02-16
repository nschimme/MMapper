// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "WeatherRenderer.h"

#include "../configuration/configuration.h"
#include "../global/utils.h"
#include "../map/coordinate.h"
#include "../mapdata/mapdata.h"
#include "../opengl/legacy/Binders.h"
#include "../opengl/legacy/Legacy.h"
#include "../opengl/legacy/Shaders.h"
#include "../opengl/legacy/TF.h"
#include "../opengl/legacy/VAO.h"
#include "../opengl/legacy/VBO.h"

#include <algorithm>
#include <random>

WeatherRenderer::WeatherRenderer(OpenGL &gl, MapData &data, const MapCanvasTextures &textures)
    : m_gl{gl}
    , m_data{data}
    , m_textures{textures}
{}

WeatherRenderer::~WeatherRenderer() = default;

void WeatherRenderer::init()
{
    // Transition logic moved from MapCanvas constructor
}

void WeatherRenderer::update(float dt)
{
    const auto &settings = getConfig().canvas;
    // User setting 50 is the default (100% of game intensity)
    m_state.targetRainIntensity = m_state.gameRainIntensity
                                  * (static_cast<float>(settings.weatherRainIntensity.get())
                                     / 50.0f);
    m_state.targetSnowIntensity = m_state.gameSnowIntensity
                                  * (static_cast<float>(settings.weatherSnowIntensity.get())
                                     / 50.0f);
    m_state.targetCloudsIntensity = m_state.gameCloudsIntensity
                                    * (static_cast<float>(settings.weatherCloudsIntensity.get())
                                       / 50.0f);
    m_state.targetFogIntensity = m_state.gameFogIntensity
                                 * (static_cast<float>(settings.weatherFogIntensity.get()) / 50.0f);
    m_state.targetToDIntensity = static_cast<float>(settings.weatherToDIntensity.get()) / 100.0f;

    m_state.lastDt = dt;

    auto updateLevel = [dt](float &current, float target) {
        const float transitionSpeed = 0.2f; // transition over 5 seconds
        if (current < target) {
            current = std::min(target, current + dt * transitionSpeed);
        } else if (current > target) {
            current = std::max(target, current - dt * transitionSpeed);
        }
    };

    updateLevel(m_state.rainIntensity, m_state.targetRainIntensity);
    updateLevel(m_state.snowIntensity, m_state.targetSnowIntensity);
    updateLevel(m_state.cloudsIntensity, m_state.targetCloudsIntensity);
    updateLevel(m_state.fogIntensity, m_state.targetFogIntensity);
    updateLevel(m_state.todIntensity, m_state.targetToDIntensity);
    updateLevel(m_state.moonIntensity, m_state.targetMoonIntensity);

    if (m_state.timeOfDayTransition < 1.0f) {
        m_state.timeOfDayTransition = std::min(1.0f,
                                               m_state.timeOfDayTransition
                                                   + dt * 0.2f); // 5 seconds transition
    }

    m_state.animationTime += dt;
}

void WeatherRenderer::initParticles()
{
    if (m_state.initialized) {
        return;
    }
    auto &funcs = deref(m_gl.getSharedFunctions(Badge<WeatherRenderer>{}));

    // Reduced particle count: 4096 rain + 1024 snow = 5120 total.
    // We only store 3 floats per particle: x, y, life. Hash and type are derived in shaders.
    m_state.numParticles = 5120;
    std::vector<float> data;
    data.reserve(m_state.numParticles * 3);

    std::mt19937 gen(42);
    std::uniform_real_distribution<float> dis(0.0, 1.0);
    std::uniform_real_distribution<float> posDis(-20.0, 20.0);

    const auto playerPos = m_data.tryGetPosition().value_or(Coordinate{}).to_vec3();

    for (uint32_t i = 0; i < 4096; ++i) { // Rain
        data.push_back(playerPos.x + posDis(gen));
        data.push_back(playerPos.y + posDis(gen));
        data.push_back(dis(gen)); // life
    }
    for (uint32_t i = 0; i < 1024; ++i) { // Snow
        data.push_back(playerPos.x + posDis(gen));
        data.push_back(playerPos.y + posDis(gen));
        data.push_back(dis(gen)); // life
    }

    using namespace Legacy;
    const auto sharedFuncs = funcs.shared_from_this();

    auto getVbo = [&](SharedVboEnum e) -> GLuint {
        auto shared = funcs.getSharedVbos().get(e);
        if (!*shared) {
            shared->emplace(sharedFuncs);
        }
        return shared->get();
    };

    auto getVao = [&](SharedVaoEnum e) -> GLuint {
        auto shared = funcs.getSharedVaos().get(e);
        if (!*shared) {
            shared->emplace(sharedFuncs);
        }
        return shared->get();
    };

    auto getTf = [&](SharedTfEnum e) -> GLuint {
        auto shared = funcs.getSharedTfs().get(e);
        if (!*shared) {
            shared->emplace(sharedFuncs);
        }
        return shared->get();
    };

    const GLuint vbo0 = getVbo(SharedVboEnum::WeatherParticles0);
    const GLuint vbo1 = getVbo(SharedVboEnum::WeatherParticles1);
    const GLuint quadVbo = getVbo(SharedVboEnum::WeatherQuad);

    funcs.glBindBuffer(GL_ARRAY_BUFFER, vbo0);
    funcs.glBufferData(GL_ARRAY_BUFFER,
                       static_cast<GLsizeiptr>(data.size() * sizeof(float)),
                       data.data(),
                       GL_DYNAMIC_DRAW);
    funcs.glBindBuffer(GL_ARRAY_BUFFER, vbo1);
    funcs.glBufferData(GL_ARRAY_BUFFER,
                       static_cast<GLsizeiptr>(data.size() * sizeof(float)),
                       data.data(),
                       GL_DYNAMIC_DRAW);

    std::ignore = getTf(SharedTfEnum::WeatherSimulation);

    const SharedVaoEnum simVaos[] = {SharedVaoEnum::WeatherSimulation0,
                                     SharedVaoEnum::WeatherSimulation1};
    const GLuint pVbos[] = {vbo0, vbo1};

    for (int i = 0; i < 2; ++i) {
        funcs.glBindVertexArray(getVao(simVaos[i]));
        funcs.glBindBuffer(GL_ARRAY_BUFFER, pVbos[i]);
        funcs.glEnableVertexAttribArray(0); // pos
        funcs.glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
        funcs.glEnableVertexAttribArray(1); // life
        funcs.glVertexAttribPointer(1,
                                    1,
                                    GL_FLOAT,
                                    GL_FALSE,
                                    3 * sizeof(float),
                                    reinterpret_cast<void *>(2 * sizeof(float)));
    }

    // Quad for rendering
    float quad[] = {-0.5f, -0.5f, 0.5f, -0.5f, -0.5f, 0.5f, 0.5f, 0.5f};
    funcs.glBindBuffer(GL_ARRAY_BUFFER, quadVbo);
    funcs.glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);

    const SharedVaoEnum rainVaos[] = {SharedVaoEnum::WeatherRenderRain0,
                                      SharedVaoEnum::WeatherRenderRain1};
    const SharedVaoEnum snowVaos[] = {SharedVaoEnum::WeatherRenderSnow0,
                                      SharedVaoEnum::WeatherRenderSnow1};

    for (int i = 0; i < 2; ++i) {
        // Rain VAO
        funcs.glBindVertexArray(getVao(rainVaos[i]));

        funcs.glBindBuffer(GL_ARRAY_BUFFER, quadVbo);
        funcs.glEnableVertexAttribArray(0); // aQuadPos
        funcs.glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);

        funcs.glBindBuffer(GL_ARRAY_BUFFER, pVbos[i]);
        funcs.glEnableVertexAttribArray(1); // aParticlePos
        funcs.glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
        funcs.glVertexAttribDivisor(1, 1);

        funcs.glEnableVertexAttribArray(2); // aLife
        funcs.glVertexAttribPointer(2,
                                    1,
                                    GL_FLOAT,
                                    GL_FALSE,
                                    3 * sizeof(float),
                                    reinterpret_cast<void *>(2 * sizeof(float)));
        funcs.glVertexAttribDivisor(2, 1);

        // Snow VAO
        funcs.glBindVertexArray(getVao(snowVaos[i]));

        funcs.glBindBuffer(GL_ARRAY_BUFFER, quadVbo);
        funcs.glEnableVertexAttribArray(0); // aQuadPos
        funcs.glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);

        funcs.glBindBuffer(GL_ARRAY_BUFFER, pVbos[i]);
        funcs.glEnableVertexAttribArray(1); // aParticlePos
        funcs.glVertexAttribPointer(1,
                                    2,
                                    GL_FLOAT,
                                    GL_FALSE,
                                    3 * sizeof(float),
                                    reinterpret_cast<void *>(4096 * 3 * sizeof(float))); // Offset to snow
        funcs.glVertexAttribDivisor(1, 1);

        funcs.glEnableVertexAttribArray(2); // aLife
        funcs.glVertexAttribPointer(2,
                                    1,
                                    GL_FLOAT,
                                    GL_FALSE,
                                    3 * sizeof(float),
                                    reinterpret_cast<void *>((4096 * 3 + 2) * sizeof(float)));
        funcs.glVertexAttribDivisor(2, 1);
    }

    funcs.glBindVertexArray(0);
    m_state.initialized = true;
}

Color WeatherRenderer::calculateTimeOfDayColor() const
{
    auto getColor = [this](MumeTimeEnum t) -> Color {
        switch (t) {
        case MumeTimeEnum::DAWN:
            return Color(0.6f, 0.55f, 0.5f, 0.1f);

        case MumeTimeEnum::DUSK:
            return Color(0.3f, 0.3f, 0.45f, 0.15f);

        case MumeTimeEnum::NIGHT: {
            const float moon = m_state.moonIntensity;
            const Color baseNight(0.05f, 0.05f, 0.2f, 0.3f);
            const Color moonNight(0.2f, 0.2f, 0.4f, 0.2f);
            return Color(baseNight.getVec4() * (1.0f - moon) + moonNight.getVec4() * moon);
        }

        case MumeTimeEnum::DAY:
        case MumeTimeEnum::UNKNOWN:
        default:
            return Color(1.0f, 1.0f, 1.0f, 0.0f);
        }
    };

    const glm::vec4 c1 = getColor(m_state.oldTimeOfDay).getVec4();
    const glm::vec4 c2 = getColor(m_state.currentTimeOfDay).getVec4();
    const float t = m_state.timeOfDayTransition;

    return Color(c1.r * (1.0f - t) + c2.r * t,
                 c1.g * (1.0f - t) + c2.g * t,
                 c1.b * (1.0f - t) + c2.b * t,
                 (c1.a * (1.0f - t) + c2.a * t) * (m_state.todIntensity * 2.0f));
}

void WeatherRenderer::renderParticles(const glm::mat4 &viewProj)
{
    const bool hasParticles = m_state.rainIntensity > 0.0f || m_state.snowIntensity > 0.0f
                              || m_state.targetRainIntensity > 0.0f
                              || m_state.targetSnowIntensity > 0.0f;

    if (!hasParticles) {
        return;
    }

    initParticles();

    auto &sharedFuncs = m_gl.getSharedFunctions(Badge<WeatherRenderer>{});
    Legacy::Functions &funcs = deref(sharedFuncs);

    const auto playerPos = m_data.tryGetPosition().value_or(Coordinate{}).to_vec3();
    const bool want3D = getConfig().canvas.advanced.use3D.get();
    const float zScale = want3D ? getConfig().canvas.advanced.layerHeight.getFloat() : 7.0f;

    GLRenderState::Uniforms uniforms;
    auto &w = uniforms.weather;
    w.viewProj = viewProj;
    w.invViewProj = glm::inverse(viewProj);
    w.playerPos = glm::vec4(playerPos, zScale);
    w.intensities = glm::vec4(m_state.rainIntensity,
                              m_state.snowIntensity,
                              m_state.cloudsIntensity,
                              m_state.fogIntensity);
    w.todColor = calculateTimeOfDayColor().getVec4();
    w.timeInfo = glm::vec4(m_state.animationTime, m_state.lastDt, 0.0f, 0.0f);

    using namespace Legacy;
    const auto buffer = SharedVboEnum::WeatherBlock;
    const auto sharedUbo = funcs.getSharedVbos().get(buffer);
    VBO &vboUbo = deref(sharedUbo);
    if (!vboUbo) {
        vboUbo.emplace(sharedFuncs);
    }
    std::ignore = funcs.setUbo(vboUbo.get(),
                               std::vector<GLRenderState::Uniforms::Weather>{w},
                               BufferUsageEnum::DYNAMIC_DRAW);
    funcs.glBindBufferBase(GL_UNIFORM_BUFFER, buffer, vboUbo.get());

    // Pass 1: Simulation
    {
        const float rainMax = std::max(m_state.rainIntensity, m_state.targetRainIntensity);
        const float snowMax = std::max(m_state.snowIntensity, m_state.targetSnowIntensity);
        const GLsizei simRainCount = static_cast<GLsizei>(std::ceil(rainMax * 2048.0f));
        const GLsizei simSnowCount = static_cast<GLsizei>(std::ceil(snowMax * 512.0f));

        if (simRainCount > 0 || simSnowCount > 0) {
            auto &prog = deref(funcs.getShaderPrograms().getParticleSimulationShader());
            auto binder = prog.bind();
            prog.setUniforms(viewProj, uniforms);

            const SharedVaoEnum simVaos[] = {SharedVaoEnum::WeatherSimulation0,
                                             SharedVaoEnum::WeatherSimulation1};
            const SharedVboEnum pVbos[] = {SharedVboEnum::WeatherParticles0,
                                           SharedVboEnum::WeatherParticles1};

            funcs.glEnable(GL_RASTERIZER_DISCARD);
            funcs.glBindVertexArray(funcs.getSharedVaos().get(simVaos[m_state.currentBuffer])->get());
            funcs.glBindTransformFeedback(GL_TRANSFORM_FEEDBACK,
                                          funcs.getSharedTfs().get(SharedTfEnum::WeatherSimulation)->get());

            const GLuint outputVbo = funcs.getSharedVbos().get(pVbos[1 - m_state.currentBuffer])->get();

            // Simulate Rain
            if (simRainCount > 0) {
                funcs.glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, outputVbo);
                funcs.glBeginTransformFeedback(GL_POINTS);
                funcs.glDrawArrays(GL_POINTS, 0, simRainCount);
                funcs.glEndTransformFeedback();
            }

            // Simulate Snow
            if (simSnowCount > 0) {
                const GLintptr snowOffset = 4096 * 3 * sizeof(float);
                const GLsizeiptr snowSize = 1024 * 3 * sizeof(float);
                funcs.glBindBufferRange(GL_TRANSFORM_FEEDBACK_BUFFER, 0, outputVbo, snowOffset, snowSize);
                funcs.glBeginTransformFeedback(GL_POINTS);
                funcs.glDrawArrays(GL_POINTS, 4096, simSnowCount);
                funcs.glEndTransformFeedback();
            }

            funcs.glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, 0);
            funcs.glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, 0);
            funcs.glDisable(GL_RASTERIZER_DISCARD);
        }

        m_state.currentBuffer = 1 - m_state.currentBuffer;
    }

    // Pass 3: Particles
    if (m_state.rainIntensity > 0.0f || m_state.snowIntensity > 0.0f) {
        auto &prog = deref(funcs.getShaderPrograms().getParticleRenderShader());
        auto binder = prog.bind();
        prog.setUniforms(viewProj, uniforms);

        const auto rs
            = GLRenderState().withBlend(BlendModeEnum::MAX_ALPHA).withDepthFunction(std::nullopt);
        Legacy::RenderStateBinder renderStateBinder(funcs, funcs.getTexLookup(), rs);

        using namespace Legacy;
        const SharedVaoEnum rainVaos[] = {SharedVaoEnum::WeatherRenderRain0,
                                          SharedVaoEnum::WeatherRenderRain1};
        const SharedVaoEnum snowVaos[] = {SharedVaoEnum::WeatherRenderSnow0,
                                          SharedVaoEnum::WeatherRenderSnow1};

        // Rain
        const GLsizei rainCount = std::min(4096, static_cast<GLsizei>(std::ceil(m_state.rainIntensity * 2048.0f)));
        if (rainCount > 0) {
            prog.setFloat("uType", 0.0f);
            prog.setInt("uInstanceOffset", 0);
            funcs.glBindVertexArray(funcs.getSharedVaos().get(rainVaos[m_state.currentBuffer])->get());
            funcs.glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, rainCount);
        }

        // Snow
        const GLsizei snowCount = std::min(1024, static_cast<GLsizei>(std::ceil(m_state.snowIntensity * 512.0f)));
        if (snowCount > 0) {
            prog.setFloat("uType", 1.0f);
            prog.setInt("uInstanceOffset", 4096);
            funcs.glBindVertexArray(funcs.getSharedVaos().get(snowVaos[m_state.currentBuffer])->get());
            funcs.glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, snowCount);
        }

        funcs.glBindVertexArray(0);
    }
}

void WeatherRenderer::renderAtmosphere(const glm::mat4 &viewProj)
{
    const Color todColor = calculateTimeOfDayColor();
    const bool hasAtmosphere = m_state.cloudsIntensity > 0.0f || m_state.fogIntensity > 0.0f;

    if (!hasAtmosphere && todColor.getAlpha() == 0) {
        return;
    }

    auto &sharedFuncs = m_gl.getSharedFunctions(Badge<WeatherRenderer>{});
    Legacy::Functions &funcs = deref(sharedFuncs);

    const auto playerPos = m_data.tryGetPosition().value_or(Coordinate{}).to_vec3();
    const bool want3D = getConfig().canvas.advanced.use3D.get();
    const float zScale = want3D ? getConfig().canvas.advanced.layerHeight.getFloat() : 7.0f;

    GLRenderState::Uniforms uniforms;
    auto &w = uniforms.weather;
    w.viewProj = viewProj;
    w.invViewProj = glm::inverse(viewProj);
    w.playerPos = glm::vec4(playerPos, zScale);
    w.intensities = glm::vec4(m_state.rainIntensity,
                              m_state.snowIntensity,
                              m_state.cloudsIntensity,
                              m_state.fogIntensity);
    w.todColor = todColor.getVec4();
    w.timeInfo = glm::vec4(m_state.animationTime, m_state.lastDt, 0.0f, 0.0f);

    using namespace Legacy;
    const auto buffer = SharedVboEnum::WeatherBlock;
    auto sharedVbo = funcs.getSharedVbos().get(buffer);
    VBO &vboUbo = deref(sharedVbo);
    std::ignore = funcs.setUbo(vboUbo.get(),
                               std::vector<GLRenderState::Uniforms::Weather>{w},
                               BufferUsageEnum::DYNAMIC_DRAW);
    funcs.glBindBufferBase(GL_UNIFORM_BUFFER, buffer, vboUbo.get());

    // Atmosphere
    {
        auto &prog = deref(funcs.getShaderPrograms().getAtmosphereShader());
        auto binder = prog.bind();
        prog.setUniforms(viewProj, uniforms);

        const auto rs = GLRenderState()
                            .withBlend(BlendModeEnum::TRANSPARENCY)
                            .withDepthFunction(std::nullopt)
                            .withTexture0(m_textures.weather_noise->getId());
        Legacy::RenderStateBinder renderStateBinder(funcs, funcs.getTexLookup(), rs);

        Legacy::SharedVao shared = funcs.getSharedVaos().get(Legacy::SharedVaoEnum::EmptyVao);
        Legacy::VAO &vao = deref(shared);
        if (!vao) {
            vao.emplace(sharedFuncs);
        }
        funcs.glBindVertexArray(vao.get());
        funcs.glDrawArrays(GL_TRIANGLES, 0, 3);
        funcs.glBindVertexArray(0);
    }
}
