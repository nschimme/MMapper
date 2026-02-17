// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "WeatherRenderer.h"

#include "../configuration/configuration.h"
#include "../global/Badge.h"
#include "../global/random.h"
#include "../global/utils.h"
#include "../map/coordinate.h"
#include "../mapdata/mapdata.h"
#include "../opengl/OpenGL.h"
#include "../opengl/OpenGLTypes.h"
#include "../opengl/legacy/Binders.h"
#include "../opengl/legacy/Legacy.h"
#include "../opengl/legacy/Shaders.h"
#include "../opengl/legacy/TF.h"
#include "../opengl/legacy/VAO.h"
#include "../opengl/legacy/VBO.h"
#include "Textures.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

#include <glm/glm.hpp>

namespace {
constexpr float TRANSITION_DURATION = 2.0f;
constexpr float ROOM_Z_SCALE = 7.f;

template<typename T>
T my_lerp(T a, T b, float t)
{
    return a + t * (b - a);
}

float get_random_float()
{
    return static_cast<float>(getRandom(1000000)) / 1000000.0f;
}
} // namespace

WeatherRenderer::WeatherRenderer(OpenGL &gl,
                                 MapData &data,
                                 const MapCanvasTextures &textures,
                                 GameObserver &observer,
                                 std::function<void(bool)> setAnimating)
    : m_gl(gl)
    , m_data(data)
    , m_textures(textures)
    , m_observer(observer)
    , m_setAnimating(std::move(setAnimating))
{
    const auto &canvasSettings = getConfig().canvas;

    auto updateFromGame = [this]() {
        auto w = m_observer.getWeather();
        auto f = m_observer.getFog();

        m_state.gameRainIntensity = 0.0f;
        m_state.gameSnowIntensity = 0.0f;
        m_state.gameCloudsIntensity = 0.0f;
        m_state.gameFogIntensity = 0.0f;

        switch (w) {
        case PromptWeatherEnum::NICE:
            break;
        case PromptWeatherEnum::CLOUDS:
            m_state.gameCloudsIntensity = 0.5f;
            break;
        case PromptWeatherEnum::RAIN:
            m_state.gameCloudsIntensity = 0.8f;
            m_state.gameRainIntensity = 0.5f;
            break;
        case PromptWeatherEnum::HEAVY_RAIN:
            m_state.gameCloudsIntensity = 1.0f;
            m_state.gameRainIntensity = 1.0f;
            break;
        case PromptWeatherEnum::SNOW:
            m_state.gameCloudsIntensity = 0.8f;
            m_state.gameSnowIntensity = 0.8f;
            break;
        }

        switch (f) {
        case PromptFogEnum::NO_FOG:
            break;
        case PromptFogEnum::LIGHT_FOG:
            m_state.gameFogIntensity = 0.5f;
            break;
        case PromptFogEnum::HEAVY_FOG:
            m_state.gameFogIntensity = 1.0f;
            break;
        }
    };

    updateFromGame();

    m_state.moonVisibility = m_observer.getMoonVisibility();
    m_state.targetMoonIntensity = (m_state.moonVisibility == MumeMoonVisibilityEnum::BRIGHT) ? 1.0f
                                                                                            : 0.0f;

    auto updateTargets = [this, &canvasSettings]() {
        m_state.targetRainIntensity = m_state.gameRainIntensity
                                      * (static_cast<float>(canvasSettings.weatherRainIntensity.get())
                                         / 50.0f);
        m_state.targetSnowIntensity = m_state.gameSnowIntensity
                                      * (static_cast<float>(canvasSettings.weatherSnowIntensity.get())
                                         / 50.0f);
        m_state.targetCloudsIntensity = m_state.gameCloudsIntensity
                                        * (static_cast<float>(canvasSettings.weatherCloudsIntensity.get())
                                           / 50.0f);
        m_state.targetFogIntensity = m_state.gameFogIntensity
                                     * (static_cast<float>(canvasSettings.weatherFogIntensity.get())
                                        / 50.0f);
        m_state.targetToDIntensity = (static_cast<float>(canvasSettings.weatherToDIntensity.get())
                                      / 50.0f);
    };

    updateTargets();

    m_state.currentTimeOfDay = m_observer.getTimeOfDay();
    m_state.oldTimeOfDay = m_state.currentTimeOfDay;
    m_state.todIntensityStart = m_state.targetToDIntensity;

    m_state.rainIntensityStart = m_state.targetRainIntensity;
    m_state.snowIntensityStart = m_state.targetSnowIntensity;
    m_state.cloudsIntensityStart = m_state.targetCloudsIntensity;
    m_state.fogIntensityStart = m_state.targetFogIntensity;
    m_state.moonIntensityStart = m_state.targetMoonIntensity;

    auto lerpCurrentIntensities = [this]() {
        float t = (m_state.animationTime - m_state.weatherTransitionStartTime) / TRANSITION_DURATION;
        float factor = std::clamp(t, 0.0f, 1.0f);
        m_state.rainIntensityStart = my_lerp(m_state.rainIntensityStart, m_state.targetRainIntensity, factor);
        m_state.snowIntensityStart = my_lerp(m_state.snowIntensityStart, m_state.targetSnowIntensity, factor);
        m_state.cloudsIntensityStart = my_lerp(m_state.cloudsIntensityStart, m_state.targetCloudsIntensity, factor);
        m_state.fogIntensityStart = my_lerp(m_state.fogIntensityStart, m_state.targetFogIntensity, factor);
    };

    m_observer.sig2_weatherChanged.connect(
        m_lifetime, [this, lerpCurrentIntensities, updateFromGame, updateTargets](PromptWeatherEnum) {
            lerpCurrentIntensities();
            updateFromGame();
            updateTargets();
            m_state.weatherTransitionStartTime = m_state.animationTime;
            m_setAnimating(true);
        });

    m_observer.sig2_fogChanged.connect(
        m_lifetime, [this, lerpCurrentIntensities, updateFromGame, updateTargets](PromptFogEnum) {
            lerpCurrentIntensities();
            updateFromGame();
            updateTargets();
            m_state.weatherTransitionStartTime = m_state.animationTime;
            m_setAnimating(true);
        });

    m_observer.sig2_timeOfDayChanged.connect(m_lifetime, [this](MumeTimeEnum tod) {
        if (tod == m_state.currentTimeOfDay) {
            return;
        }

        float t = (m_state.animationTime - m_state.todTransitionStartTime) / TRANSITION_DURATION;
        float factor = std::clamp(t, 0.0f, 1.0f);
        m_state.todIntensityStart = my_lerp(m_state.todIntensityStart, m_state.targetToDIntensity, factor);
        m_state.moonIntensityStart = my_lerp(m_state.moonIntensityStart, m_state.targetMoonIntensity, factor);

        m_state.oldTimeOfDay = m_state.currentTimeOfDay;
        m_state.currentTimeOfDay = tod;
        m_state.targetToDIntensity = (tod == MumeTimeEnum::DAY) ? 0.0f : 1.0f;
        m_state.todTransitionStartTime = m_state.animationTime;
        m_setAnimating(true);
    });

    m_observer.sig2_moonVisibilityChanged.connect(m_lifetime, [this](MumeMoonVisibilityEnum visibility) {
        if (visibility == m_state.moonVisibility) {
            return;
        }
        float t = (m_state.animationTime - m_state.todTransitionStartTime) / TRANSITION_DURATION;
        float factor = std::clamp(t, 0.0f, 1.0f);
        m_state.moonIntensityStart = my_lerp(m_state.moonIntensityStart, m_state.targetMoonIntensity, factor);
        m_state.todIntensityStart = my_lerp(m_state.todIntensityStart, m_state.targetToDIntensity, factor);

        m_state.moonVisibility = visibility;
        m_state.targetMoonIntensity = (visibility == MumeMoonVisibilityEnum::BRIGHT) ? 1.0f : 0.0f;
        m_state.todTransitionStartTime = m_state.animationTime;
        m_setAnimating(true);
    });

    auto onSettingChanged = [this, lerpCurrentIntensities, updateTargets]() {
        lerpCurrentIntensities();
        updateTargets();
        m_state.weatherTransitionStartTime = m_state.animationTime;
        m_setAnimating(true);
    };

    auto onToDSettingChanged = [this, updateTargets]() {
        float t = (m_state.animationTime - m_state.todTransitionStartTime) / TRANSITION_DURATION;
        float factor = std::clamp(t, 0.0f, 1.0f);
        m_state.todIntensityStart = my_lerp(m_state.todIntensityStart, m_state.targetToDIntensity, factor);

        updateTargets();
        m_state.todTransitionStartTime = m_state.animationTime;
        m_setAnimating(true);
    };

    canvasSettings.weatherRainIntensity.registerChangeCallback(m_lifetime, onSettingChanged);
    canvasSettings.weatherSnowIntensity.registerChangeCallback(m_lifetime, onSettingChanged);
    canvasSettings.weatherCloudsIntensity.registerChangeCallback(m_lifetime, onSettingChanged);
    canvasSettings.weatherFogIntensity.registerChangeCallback(m_lifetime, onSettingChanged);
    canvasSettings.weatherToDIntensity.registerChangeCallback(m_lifetime, onToDSettingChanged);
}

WeatherRenderer::~WeatherRenderer() = default;

void WeatherRenderer::init()
{
    initParticles();
}

void WeatherRenderer::initParticles()
{
    if (m_state.initialized) {
        return;
    }

    auto gl_funcs = m_gl.getSharedFunctions(Badge<WeatherRenderer>{});
    auto &funcs = *gl_funcs;

    auto tf = funcs.getSharedTfs().get(Legacy::SharedTfEnum::WeatherSimulation);
    if (!*tf) {
        tf->emplace(gl_funcs);
    }

    const auto buffer0 = Legacy::SharedVboEnum::WeatherParticles0;
    const auto buffer1 = Legacy::SharedVboEnum::WeatherParticles1;

    const size_t totalParticles = 5120; // 4096 rain + 1024 snow
    std::vector<float> initialData(totalParticles * 3, 0.0f);
    for (size_t i = 0; i < totalParticles; ++i) {
        initialData[i * 3 + 0] = get_random_float() * 40.0f - 20.0f;
        initialData[i * 3 + 1] = get_random_float() * 40.0f - 20.0f;
        initialData[i * 3 + 2] = get_random_float() * 1.0f;
    }

    auto vbo0 = funcs.getSharedVbos().get(buffer0);
    auto vbo1 = funcs.getSharedVbos().get(buffer1);

    if (!*vbo0) vbo0->emplace(gl_funcs);
    if (!*vbo1) vbo1->emplace(gl_funcs);

    funcs.glBindBuffer(GL_ARRAY_BUFFER, vbo0->get());
    funcs.glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(initialData.size() * sizeof(float)), initialData.data(), GL_STREAM_DRAW);
    funcs.glBindBuffer(GL_ARRAY_BUFFER, vbo1->get());
    funcs.glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(initialData.size() * sizeof(float)), initialData.data(), GL_STREAM_DRAW);
    funcs.glBindBuffer(GL_ARRAY_BUFFER, 0);

    // Initialize VAOs
    auto vaoSim0 = funcs.getSharedVaos().get(Legacy::SharedVaoEnum::WeatherSimulation0);
    if (!*vaoSim0) vaoSim0->emplace(gl_funcs);
    funcs.glBindVertexArray(vaoSim0->get());
    funcs.glBindBuffer(GL_ARRAY_BUFFER, vbo0->get());
    funcs.glEnableVertexAttribArray(0);
    funcs.glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
    funcs.glEnableVertexAttribArray(1);
    funcs.glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, 3 * sizeof(float), reinterpret_cast<void *>(2 * sizeof(float)));

    auto vaoSim1 = funcs.getSharedVaos().get(Legacy::SharedVaoEnum::WeatherSimulation1);
    if (!*vaoSim1) vaoSim1->emplace(gl_funcs);
    funcs.glBindVertexArray(vaoSim1->get());
    funcs.glBindBuffer(GL_ARRAY_BUFFER, vbo1->get());
    funcs.glEnableVertexAttribArray(0);
    funcs.glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
    funcs.glEnableVertexAttribArray(1);
    funcs.glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, 3 * sizeof(float), reinterpret_cast<void *>(2 * sizeof(float)));

    auto setupRenderVao = [&](Legacy::SharedVaoEnum vaoEnum, Legacy::SharedVboEnum vboEnum, size_t offset) {
        auto vao = funcs.getSharedVaos().get(vaoEnum);
        if (!*vao) vao->emplace(gl_funcs);
        auto vbo = funcs.getSharedVbos().get(vboEnum);
        if (!*vbo) vbo->emplace(gl_funcs);

        funcs.glBindVertexArray(vao->get());
        funcs.glBindBuffer(GL_ARRAY_BUFFER, vbo->get());
        funcs.glEnableVertexAttribArray(0);
        funcs.glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 3 * sizeof(float), reinterpret_cast<void *>(offset));
        funcs.glVertexAttribDivisor(0, 1);
        funcs.glEnableVertexAttribArray(1);
        funcs.glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, 3 * sizeof(float), reinterpret_cast<void *>(offset + 2 * sizeof(float)));
        funcs.glVertexAttribDivisor(1, 1);
    };

    setupRenderVao(Legacy::SharedVaoEnum::WeatherRenderRain0, buffer0, 0);
    setupRenderVao(Legacy::SharedVaoEnum::WeatherRenderRain1, buffer1, 0);
    setupRenderVao(Legacy::SharedVaoEnum::WeatherRenderSnow0, buffer0, 4096 * 3 * sizeof(float));
    setupRenderVao(Legacy::SharedVaoEnum::WeatherRenderSnow1, buffer1, 4096 * 3 * sizeof(float));

    funcs.glBindVertexArray(0);

    m_state.numParticles = static_cast<uint32_t>(totalParticles);
    m_state.initialized = true;
}

void WeatherRenderer::update(float dt)
{
    const auto &canvasSettings = getConfig().canvas;
    m_state.targetRainIntensity = m_state.gameRainIntensity
                                  * (static_cast<float>(canvasSettings.weatherRainIntensity.get()) / 50.0f);
    m_state.targetSnowIntensity = m_state.gameSnowIntensity
                                  * (static_cast<float>(canvasSettings.weatherSnowIntensity.get()) / 50.0f);
    m_state.targetCloudsIntensity = m_state.gameCloudsIntensity
                                    * (static_cast<float>(canvasSettings.weatherCloudsIntensity.get()) / 50.0f);
    m_state.targetFogIntensity = m_state.gameFogIntensity
                                 * (static_cast<float>(canvasSettings.weatherFogIntensity.get()) / 50.0f);
    m_state.targetToDIntensity = (static_cast<float>(canvasSettings.weatherToDIntensity.get()) / 50.0f);

    m_state.lastDt = dt;
    m_state.animationTime += dt;

    bool transitioning = (m_state.animationTime - m_state.weatherTransitionStartTime < TRANSITION_DURATION)
                         || (m_state.animationTime - m_state.todTransitionStartTime < TRANSITION_DURATION);

    bool hasWeather = m_state.targetRainIntensity > 0.0f || m_state.targetSnowIntensity > 0.0f
                      || m_state.targetCloudsIntensity > 0.0f || m_state.targetFogIntensity > 0.0f
                      || m_state.rainIntensityStart > 0.0f || m_state.snowIntensityStart > 0.0f
                      || m_state.cloudsIntensityStart > 0.0f || m_state.fogIntensityStart > 0.0f;

    bool hasToD = m_state.currentTimeOfDay != MumeTimeEnum::DAY || m_state.oldTimeOfDay != MumeTimeEnum::DAY;

    m_setAnimating(transitioning || hasWeather || hasToD);
}

void WeatherRenderer::updateUbo(const glm::mat4 &viewProj)
{
    if (utils::isSameFloat(m_state.lastUboUploadTime, m_state.animationTime)) {
        return;
    }

    auto gl_funcs = m_gl.getSharedFunctions(Badge<WeatherRenderer>{});
    auto &funcs = *gl_funcs;
    const auto buffer = Legacy::SharedVboEnum::WeatherBlock;
    const auto vboUbo = funcs.getSharedVbos().get(buffer);
    if (!*vboUbo) vboUbo->emplace(gl_funcs);

    GLRenderState::Uniforms::Weather w;
    w.viewProj = viewProj;
    w.invViewProj = glm::inverse(viewProj);

    const auto playerPosCoord = m_data.tryGetPosition().value_or(Coordinate{0, 0, 0});
    w.playerPos = glm::vec4(static_cast<float>(playerPosCoord.x), static_cast<float>(playerPosCoord.y),
                            static_cast<float>(playerPosCoord.z), ROOM_Z_SCALE);

    w.intensitiesStart = glm::vec4(m_state.rainIntensityStart, m_state.snowIntensityStart,
                                   m_state.cloudsIntensityStart, m_state.fogIntensityStart);
    w.intensitiesTarget = glm::vec4(m_state.targetRainIntensity, m_state.targetSnowIntensity,
                                    m_state.targetCloudsIntensity, m_state.targetFogIntensity);

    auto getToDColor = [this](MumeTimeEnum tod, float moonIntensity, float todIntensity) {
        glm::vec4 color = glm::vec4(0.0f);
        switch (tod) {
        case MumeTimeEnum::UNKNOWN:
        case MumeTimeEnum::DAY:
            break;
        case MumeTimeEnum::NIGHT:
            color = glm::vec4(0.05f, 0.05f, 0.2f, 0.6f + 0.2f * moonIntensity);
            break;
        case MumeTimeEnum::DAWN:
            color = glm::vec4(0.4f, 0.3f, 0.2f, 0.3f);
            break;
        case MumeTimeEnum::DUSK:
            color = glm::vec4(0.3f, 0.2f, 0.4f, 0.4f);
            break;
        }
        color.a *= todIntensity; // default 50% slider -> todIntensity=1.0 -> same as old behavior
        return color;
    };

    w.todColorStart = getToDColor(m_state.oldTimeOfDay, m_state.moonIntensityStart, m_state.todIntensityStart);
    w.todColorTarget = getToDColor(m_state.currentTimeOfDay, m_state.targetMoonIntensity, m_state.targetToDIntensity);

    w.transitionStart = glm::vec4(m_state.weatherTransitionStartTime, m_state.todTransitionStartTime, 0.0f, 0.0f);
    w.timeInfo = glm::vec4(m_state.animationTime, m_state.lastDt, 0.0f, 0.0f);

    funcs.glBindBuffer(GL_UNIFORM_BUFFER, vboUbo->get());
    funcs.glBufferData(GL_UNIFORM_BUFFER, sizeof(w), &w, GL_DYNAMIC_DRAW);
    funcs.glBindBufferBase(GL_UNIFORM_BUFFER, static_cast<GLuint>(buffer), vboUbo->get());

    m_state.lastUboUploadTime = m_state.animationTime;
}

void WeatherRenderer::renderParticles(const glm::mat4 &viewProj)
{
    const float rainMax = std::max(m_state.rainIntensityStart, m_state.targetRainIntensity);
    const float snowMax = std::max(m_state.snowIntensityStart, m_state.targetSnowIntensity);

    if (rainMax <= 0.0f && snowMax <= 0.0f) {
        return;
    }

    initParticles();
    updateUbo(viewProj);

    auto gl_funcs = m_gl.getSharedFunctions(Badge<WeatherRenderer>{});
    auto &funcs = *gl_funcs;

    const auto bufferOut = (m_state.currentBuffer == 0) ? Legacy::SharedVboEnum::WeatherParticles1
                                                        : Legacy::SharedVboEnum::WeatherParticles0;
    const auto vboOut = funcs.getSharedVbos().get(bufferOut);
    if (!*vboOut) vboOut->emplace(gl_funcs);

    const auto rs = m_gl.getDefaultRenderState().withBlend(BlendModeEnum::MAX_ALPHA);
    Legacy::RenderStateBinder rsBinder(funcs, funcs.getTexLookup(), rs);

    // Pass 1: Simulation
    {
        auto &prog = deref(funcs.getShaderPrograms().getParticleSimulationShader());
        auto binder = prog.bind();
        GLRenderState::Uniforms uniforms;
        prog.setUniforms(viewProj, uniforms);

        auto vaoEnum = (m_state.currentBuffer == 0) ? Legacy::SharedVaoEnum::WeatherSimulation0
                                                     : Legacy::SharedVaoEnum::WeatherSimulation1;
        auto vao = funcs.getSharedVaos().get(vaoEnum);
        if (!*vao) vao->emplace(gl_funcs);
        Legacy::VAOBinder vaoBinder(funcs, vao);

        auto tf = funcs.getSharedTfs().get(Legacy::SharedTfEnum::WeatherSimulation);
        if (!*tf) tf->emplace(gl_funcs);
        funcs.glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, vboOut->get());
        Legacy::TransformFeedbackBinder tfBinder(funcs, tf, GL_POINTS);

        funcs.glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(m_state.numParticles));
    }

    // Pass 2: Rendering
    {
        auto &prog = deref(funcs.getShaderPrograms().getParticleRenderShader());
        auto binder = prog.bind();
        GLRenderState::Uniforms uniforms;
        prog.setUniforms(viewProj, uniforms);

        // Rain
        const GLsizei rainCount = std::min(4096, static_cast<GLsizei>(std::ceil(rainMax * 2048.0f)));
        if (rainCount > 0) {
            auto vaoRainEnum = (m_state.currentBuffer == 0) ? Legacy::SharedVaoEnum::WeatherRenderRain1
                                                             : Legacy::SharedVaoEnum::WeatherRenderRain0;
            auto vao = funcs.getSharedVaos().get(vaoRainEnum);
            if (!*vao) vao->emplace(gl_funcs);
            Legacy::VAOBinder vaoBinder(funcs, vao);
            prog.setFloat("uType", 0.0f);
            prog.setInt("uInstanceOffset", 0);
            funcs.glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, rainCount);
        }

        // Snow
        const GLsizei snowCount = std::min(1024, static_cast<GLsizei>(std::ceil(snowMax * 1024.0f)));
        if (snowCount > 0) {
            auto vaoSnowEnum = (m_state.currentBuffer == 0) ? Legacy::SharedVaoEnum::WeatherRenderSnow1
                                                             : Legacy::SharedVaoEnum::WeatherRenderSnow0;
            auto vao = funcs.getSharedVaos().get(vaoSnowEnum);
            if (!*vao) vao->emplace(gl_funcs);
            Legacy::VAOBinder vaoBinder(funcs, vao);
            prog.setFloat("uType", 1.0f);
            prog.setInt("uInstanceOffset", 4096);
            funcs.glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, snowCount);
        }
    }

    m_state.currentBuffer = 1 - m_state.currentBuffer;
}

void WeatherRenderer::renderAtmosphere(const glm::mat4 &viewProj)
{
    const float cloudMax = std::max(m_state.cloudsIntensityStart, m_state.targetCloudsIntensity);
    const float fogMax = std::max(m_state.fogIntensityStart, m_state.targetFogIntensity);

    if (cloudMax <= 0.0f && fogMax <= 0.0f && m_state.currentTimeOfDay == MumeTimeEnum::DAY
        && m_state.oldTimeOfDay == MumeTimeEnum::DAY) {
        return;
    }

    updateUbo(viewProj);

    auto gl_funcs = m_gl.getSharedFunctions(Badge<WeatherRenderer>{});
    auto &funcs = *gl_funcs;
    auto &prog = deref(funcs.getShaderPrograms().getAtmosphereShader());
    auto binder = prog.bind();

    const auto rs = m_gl.getDefaultRenderState().withBlend(BlendModeEnum::TRANSPARENCY);
    Legacy::RenderStateBinder rsBinder(funcs, funcs.getTexLookup(), rs);

    GLRenderState::Uniforms uniforms;
    prog.setUniforms(viewProj, uniforms);

    auto emptyVao = funcs.getSharedVaos().get(Legacy::SharedVaoEnum::EmptyVao);
    if (!*emptyVao) emptyVao->emplace(gl_funcs);
    Legacy::VAOBinder vaoBinder(funcs, emptyVao);

    funcs.glActiveTexture(GL_TEXTURE0);
    funcs.glBindTexture(GL_TEXTURE_2D, m_textures.weather_noise->get()->textureId());
    prog.setInt("uNoiseTex", 0);

    funcs.glDrawArrays(GL_TRIANGLES, 0, 3);
}
