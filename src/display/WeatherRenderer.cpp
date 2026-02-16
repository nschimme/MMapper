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

    // We use 8192 rain and 2048 snow particles to allow for 2x intensity (setting 100/50)
    m_state.numParticles = 10240;
    std::vector<float> data;
    data.reserve(m_state.numParticles * 5);

    std::mt19937 gen(42);
    std::uniform_real_distribution<float> dis(0.0, 1.0);
    std::uniform_real_distribution<float> posDis(-20.0, 20.0);

    const auto playerPos = m_data.tryGetPosition().value_or(Coordinate{}).to_vec3();

    for (uint32_t i = 0; i < 8192; ++i) { // Rain
        data.push_back(playerPos.x + posDis(gen));
        data.push_back(playerPos.y + posDis(gen));
        data.push_back(dis(gen)); // hash
        data.push_back(0.0f);     // type (Rain)
        data.push_back(dis(gen)); // life
    }
    for (uint32_t i = 0; i < 2048; ++i) { // Snow
        data.push_back(playerPos.x + posDis(gen));
        data.push_back(playerPos.y + posDis(gen));
        data.push_back(dis(gen)); // hash
        data.push_back(1.0f);     // type (Snow)
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
        funcs.glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), nullptr);
        funcs.glEnableVertexAttribArray(1); // hash
        funcs.glVertexAttribPointer(1,
                                    1,
                                    GL_FLOAT,
                                    GL_FALSE,
                                    5 * sizeof(float),
                                    reinterpret_cast<void *>(2 * sizeof(float)));
        funcs.glEnableVertexAttribArray(2); // type
        funcs.glVertexAttribPointer(2,
                                    1,
                                    GL_FLOAT,
                                    GL_FALSE,
                                    5 * sizeof(float),
                                    reinterpret_cast<void *>(3 * sizeof(float)));
        funcs.glEnableVertexAttribArray(3); // life
        funcs.glVertexAttribPointer(3,
                                    1,
                                    GL_FLOAT,
                                    GL_FALSE,
                                    5 * sizeof(float),
                                    reinterpret_cast<void *>(4 * sizeof(float)));
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
        funcs.glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), nullptr);
        funcs.glVertexAttribDivisor(1, 1);

        funcs.glEnableVertexAttribArray(2); // aHash
        funcs.glVertexAttribPointer(2,
                                    1,
                                    GL_FLOAT,
                                    GL_FALSE,
                                    5 * sizeof(float),
                                    reinterpret_cast<void *>(2 * sizeof(float)));
        funcs.glVertexAttribDivisor(2, 1);

        funcs.glEnableVertexAttribArray(3); // aType
        funcs.glVertexAttribPointer(3,
                                    1,
                                    GL_FLOAT,
                                    GL_FALSE,
                                    5 * sizeof(float),
                                    reinterpret_cast<void *>(3 * sizeof(float)));
        funcs.glVertexAttribDivisor(3, 1);

        funcs.glEnableVertexAttribArray(4); // aLife
        funcs.glVertexAttribPointer(4,
                                    1,
                                    GL_FLOAT,
                                    GL_FALSE,
                                    5 * sizeof(float),
                                    reinterpret_cast<void *>(4 * sizeof(float)));
        funcs.glVertexAttribDivisor(4, 1);

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
                                    5 * sizeof(float),
                                    reinterpret_cast<void *>(8192 * 5 * sizeof(float))); // Offset to snow
        funcs.glVertexAttribDivisor(1, 1);

        funcs.glEnableVertexAttribArray(2); // aHash
        funcs.glVertexAttribPointer(2,
                                    1,
                                    GL_FLOAT,
                                    GL_FALSE,
                                    5 * sizeof(float),
                                    reinterpret_cast<void *>((8192 * 5 + 2) * sizeof(float)));
        funcs.glVertexAttribDivisor(2, 1);

        funcs.glEnableVertexAttribArray(3); // aType
        funcs.glVertexAttribPointer(3,
                                    1,
                                    GL_FLOAT,
                                    GL_FALSE,
                                    5 * sizeof(float),
                                    reinterpret_cast<void *>((8192 * 5 + 3) * sizeof(float)));
        funcs.glVertexAttribDivisor(3, 1);

        funcs.glEnableVertexAttribArray(4); // aLife
        funcs.glVertexAttribPointer(4,
                                    1,
                                    GL_FLOAT,
                                    GL_FALSE,
                                    5 * sizeof(float),
                                    reinterpret_cast<void *>((8192 * 5 + 4) * sizeof(float)));
        funcs.glVertexAttribDivisor(4, 1);
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

void WeatherRenderer::render(const glm::mat4 &viewProj)
{
    const Color todColor = calculateTimeOfDayColor();
    const bool hasWeather = m_state.rainIntensity > 0.0f || m_state.snowIntensity > 0.0f
                            || m_state.cloudsIntensity > 0.0f || m_state.fogIntensity > 0.0f;

    if (!hasWeather && todColor.getAlpha() == 0) {
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
    w.todColor = todColor.getVec4();
    const Viewport vp = funcs.getPhysicalViewport();
    w.viewport = glm::vec4(static_cast<float>(vp.offset.x),
                           static_cast<float>(vp.offset.y),
                           static_cast<float>(vp.size.x),
                           static_cast<float>(vp.size.y));
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
        funcs.glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER,
                               0,
                               funcs.getSharedVbos().get(pVbos[1 - m_state.currentBuffer])->get());

        funcs.glBeginTransformFeedback(GL_POINTS);
        funcs.glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(m_state.numParticles));
        funcs.glEndTransformFeedback();

        funcs.glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, 0);
        funcs.glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, 0);
        funcs.glDisable(GL_RASTERIZER_DISCARD);

        m_state.currentBuffer = 1 - m_state.currentBuffer;
    }

    // Pass 2: Atmosphere
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
        const GLsizei rainCount = std::min(8192, static_cast<int>(m_state.rainIntensity * 4096.0f));
        if (rainCount > 0) {
            funcs.glBindVertexArray(funcs.getSharedVaos().get(rainVaos[m_state.currentBuffer])->get());
            funcs.glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, rainCount);
        }

        // Snow
        const GLsizei snowCount = std::min(2048, static_cast<int>(m_state.snowIntensity * 1024.0f));
        if (snowCount > 0) {
            funcs.glBindVertexArray(funcs.getSharedVaos().get(snowVaos[m_state.currentBuffer])->get());
            funcs.glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, snowCount);
        }

        funcs.glBindVertexArray(0);
    }
}
