// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#include "Weather.h"

#include "../configuration/configuration.h"
#include "../global/Badge.h"
#include "../map/coordinate.h"
#include "../mapdata/mapdata.h"
#include "../opengl/OpenGL.h"
#include "../opengl/OpenGLTypes.h"
#include "../opengl/legacy/Legacy.h"
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

inline float fract(float x)
{
    return x - std::floor(x);
}

inline float lerp(float a, float b, float t)
{
    return a + t * (b - a);
}

inline float hash(float x, float y)
{
    float dot = x * 127.1f + y * 311.7f;
    return fract(std::sin(dot) * 43758.5453123f);
}

inline float noise(float x, float y, int size)
{
    float ix = std::floor(x);
    float iy = std::floor(y);
    float fx = x - ix;
    float fy = y - iy;

    float sx = fx * fx * fx * (fx * (fx * 6.0f - 15.0f) + 10.0f);
    float sy = fy * fy * fy * (fy * (fy * 6.0f - 15.0f) + 10.0f);

    auto get_hash = [&](float i, float j) {
        const float fsize = static_cast<float>(size);
        float wi = std::fmod(i, fsize);
        if (wi < 0.0f) {
            wi += fsize;
        }
        float wj = std::fmod(j, fsize);
        if (wj < 0.0f) {
            wj += fsize;
        }
        return hash(wi, wj);
    };

    float a = get_hash(ix, iy);
    float b = get_hash(ix + 1.0f, iy);
    float c = get_hash(ix, iy + 1.0f);
    float d = get_hash(ix + 1.0f, iy + 1.0f);

    return lerp(lerp(a, b, sx), lerp(c, d, sx), sy);
}

} // namespace

WeatherSystem::WeatherSystem(GameObserver &observer, AnimationManager &animationManager)
    : m_observer(observer)
    , m_animationManager(animationManager)
{
    updateFromGame();

    m_moonVisibility = m_observer.getMoonVisibility();
    m_targetMoonIntensity = (m_moonVisibility == MumeMoonVisibilityEnum::BRIGHT) ? 1.0f : 0.0f;

    m_currentTimeOfDay = m_observer.getTimeOfDay();
    m_oldTimeOfDay = m_currentTimeOfDay;
    m_gameTimeOfDayIntensity = (m_currentTimeOfDay == MumeTimeEnum::DAY) ? 0.0f : 1.0f;

    updateTargets();

    m_timeOfDayIntensityStart = m_targetTimeOfDayIntensity;
    m_rainIntensityStart = m_targetRainIntensity;
    m_snowIntensityStart = m_targetSnowIntensity;
    m_cloudsIntensityStart = m_targetCloudsIntensity;
    m_fogIntensityStart = m_targetFogIntensity;
    m_precipitationTypeStart = m_targetPrecipitationType;
    m_moonIntensityStart = m_targetMoonIntensity;

    auto lerpCurrentIntensities = [this]() {
        float t = (m_animationManager.getAnimationTime() - m_weatherTransitionStartTime)
                  / TRANSITION_DURATION;
        float factor = std::clamp(t, 0.0f, 1.0f);
        m_rainIntensityStart = my_lerp(m_rainIntensityStart, m_targetRainIntensity, factor);
        m_snowIntensityStart = my_lerp(m_snowIntensityStart, m_targetSnowIntensity, factor);
        m_cloudsIntensityStart = my_lerp(m_cloudsIntensityStart, m_targetCloudsIntensity, factor);
        m_fogIntensityStart = my_lerp(m_fogIntensityStart, m_targetFogIntensity, factor);
        m_precipitationTypeStart = my_lerp(m_precipitationTypeStart,
                                           m_targetPrecipitationType,
                                           factor);
    };

    m_observer.sig2_weatherChanged
        .connect(m_lifetime, [this, lerpCurrentIntensities](PromptWeatherEnum) {
            lerpCurrentIntensities();
            updateFromGame();
            updateTargets();
            m_weatherTransitionStartTime = m_animationManager.getAnimationTime();
            sig_stateInvalidated.invoke();
        });

    m_observer.sig2_fogChanged.connect(m_lifetime, [this, lerpCurrentIntensities](PromptFogEnum) {
        lerpCurrentIntensities();
        updateFromGame();
        updateTargets();
        m_weatherTransitionStartTime = m_animationManager.getAnimationTime();
        sig_stateInvalidated.invoke();
    });

    m_observer.sig2_timeOfDayChanged.connect(m_lifetime, [this](MumeTimeEnum timeOfDay) {
        if (timeOfDay == m_currentTimeOfDay) {
            return;
        }

        float t = (m_animationManager.getAnimationTime() - m_timeOfDayTransitionStartTime)
                  / TRANSITION_DURATION;
        float factor = std::clamp(t, 0.0f, 1.0f);
        m_timeOfDayIntensityStart = my_lerp(m_timeOfDayIntensityStart,
                                            m_targetTimeOfDayIntensity,
                                            factor);
        m_moonIntensityStart = my_lerp(m_moonIntensityStart, m_targetMoonIntensity, factor);

        m_oldTimeOfDay = m_currentTimeOfDay;
        m_currentTimeOfDay = timeOfDay;
        m_gameTimeOfDayIntensity = (timeOfDay == MumeTimeEnum::DAY) ? 0.0f : 1.0f;
        updateTargets();
        m_timeOfDayTransitionStartTime = m_animationManager.getAnimationTime();
        sig_stateInvalidated.invoke();
    });

    m_observer.sig2_moonVisibilityChanged
        .connect(m_lifetime, [this](MumeMoonVisibilityEnum visibility) {
            if (visibility == m_moonVisibility) {
                return;
            }
            float t = (m_animationManager.getAnimationTime() - m_timeOfDayTransitionStartTime)
                      / TRANSITION_DURATION;
            float factor = std::clamp(t, 0.0f, 1.0f);
            m_moonIntensityStart = my_lerp(m_moonIntensityStart, m_targetMoonIntensity, factor);
            m_timeOfDayIntensityStart = my_lerp(m_timeOfDayIntensityStart,
                                                m_targetTimeOfDayIntensity,
                                                factor);

            m_moonVisibility = visibility;
            m_targetMoonIntensity = (visibility == MumeMoonVisibilityEnum::BRIGHT) ? 1.0f : 0.0f;
            m_timeOfDayTransitionStartTime = m_animationManager.getAnimationTime();
            sig_stateInvalidated.invoke();
        });

    auto onSettingChanged = [this, lerpCurrentIntensities]() {
        lerpCurrentIntensities();
        updateTargets();
        m_weatherTransitionStartTime = m_animationManager.getAnimationTime();
        sig_stateInvalidated.invoke();
    };

    auto onTimeOfDaySettingChanged = [this]() {
        float t = (m_animationManager.getAnimationTime() - m_timeOfDayTransitionStartTime)
                  / TRANSITION_DURATION;
        float factor = std::clamp(t, 0.0f, 1.0f);
        m_timeOfDayIntensityStart = my_lerp(m_timeOfDayIntensityStart,
                                            m_targetTimeOfDayIntensity,
                                            factor);

        updateTargets();
        m_timeOfDayTransitionStartTime = m_animationManager.getAnimationTime();
        sig_stateInvalidated.invoke();
    };

    setConfig().canvas.weatherPrecipitationIntensity.registerChangeCallback(m_lifetime,
                                                                            onSettingChanged);
    setConfig().canvas.weatherAtmosphereIntensity.registerChangeCallback(m_lifetime,
                                                                         onSettingChanged);
    setConfig().canvas.weatherTimeOfDayIntensity.registerChangeCallback(m_lifetime,
                                                                        onTimeOfDaySettingChanged);
}

WeatherSystem::~WeatherSystem() = default;

void WeatherSystem::updateFromGame()
{
    auto w = m_observer.getWeather();
    auto f = m_observer.getFog();

    m_gameRainIntensity = 0.0f;
    m_gameSnowIntensity = 0.0f;
    m_gameCloudsIntensity = 0.0f;
    m_gameFogIntensity = 0.0f;

    switch (w) {
    case PromptWeatherEnum::NICE:
        break;
    case PromptWeatherEnum::CLOUDS:
        m_gameCloudsIntensity = 0.5f;
        break;
    case PromptWeatherEnum::RAIN:
        m_gameCloudsIntensity = 0.8f;
        m_gameRainIntensity = 0.5f;
        m_targetPrecipitationType = 0.0f;
        break;
    case PromptWeatherEnum::HEAVY_RAIN:
        m_gameCloudsIntensity = 1.0f;
        m_gameRainIntensity = 1.0f;
        m_targetPrecipitationType = 0.0f;
        break;
    case PromptWeatherEnum::SNOW:
        m_gameCloudsIntensity = 0.8f;
        m_gameSnowIntensity = 0.8f;
        m_targetPrecipitationType = 1.0f;
        break;
    }

    switch (f) {
    case PromptFogEnum::NO_FOG:
        break;
    case PromptFogEnum::LIGHT_FOG:
        m_gameFogIntensity = 0.5f;
        break;
    case PromptFogEnum::HEAVY_FOG:
        m_gameFogIntensity = 1.0f;
        break;
    }
}

void WeatherSystem::updateTargets()
{
    const auto &canvasSettings = getConfig().canvas;
    m_targetRainIntensity = m_gameRainIntensity
                            * (static_cast<float>(canvasSettings.weatherPrecipitationIntensity.get())
                               / 50.0f);
    m_targetSnowIntensity = m_gameSnowIntensity
                            * (static_cast<float>(canvasSettings.weatherPrecipitationIntensity.get())
                               / 50.0f);
    m_targetCloudsIntensity = m_gameCloudsIntensity
                              * (static_cast<float>(canvasSettings.weatherAtmosphereIntensity.get())
                                 / 50.0f);
    m_targetFogIntensity = m_gameFogIntensity
                           * (static_cast<float>(canvasSettings.weatherAtmosphereIntensity.get())
                              / 50.0f);
    m_targetTimeOfDayIntensity = m_gameTimeOfDayIntensity
                                 * (static_cast<float>(
                                        canvasSettings.weatherTimeOfDayIntensity.get())
                                    / 50.0f);
}

void WeatherSystem::update()
{
    updateTargets();

    const float animTime = m_animationManager.getAnimationTime();

    float wt = std::clamp((animTime - m_weatherTransitionStartTime) / TRANSITION_DURATION,
                          0.0f,
                          1.0f);
    m_currentRainIntensity = my_lerp(m_rainIntensityStart, m_targetRainIntensity, wt);
    m_currentSnowIntensity = my_lerp(m_snowIntensityStart, m_targetSnowIntensity, wt);
    m_currentCloudsIntensity = my_lerp(m_cloudsIntensityStart, m_targetCloudsIntensity, wt);
    m_currentFogIntensity = my_lerp(m_fogIntensityStart, m_targetFogIntensity, wt);

    float tt = std::clamp((animTime - m_timeOfDayTransitionStartTime) / TRANSITION_DURATION,
                          0.0f,
                          1.0f);
    m_currentTimeOfDayIntensity = my_lerp(m_timeOfDayIntensityStart, m_targetTimeOfDayIntensity, tt);
}

bool WeatherSystem::isAnimating() const
{
    const bool activePrecipitation = m_currentRainIntensity > 0.0f || m_currentSnowIntensity > 0.0f;
    const bool activeAtmosphere = m_currentCloudsIntensity > 0.0f || m_currentFogIntensity > 0.0f;
    return isTransitioning() || activePrecipitation || activeAtmosphere;
}

bool WeatherSystem::isTransitioning() const
{
    const float animTime = m_animationManager.getAnimationTime();
    const bool weatherTransitioning = (animTime - m_weatherTransitionStartTime
                                       < TRANSITION_DURATION);
    const bool timeOfDayTransitioning = (animTime - m_timeOfDayTransitionStartTime
                                         < TRANSITION_DURATION);
    return weatherTransitioning || timeOfDayTransitioning;
}

GLRenderState::Uniforms::Weather::Camera WeatherSystem::getCameraData(
    const glm::mat4 &viewProj, const Coordinate &playerPosCoord) const
{
    GLRenderState::Uniforms::Weather::Camera camera;
    camera.viewProj = viewProj;
    camera.playerPos = glm::vec4(static_cast<float>(playerPosCoord.x),
                                 static_cast<float>(playerPosCoord.y),
                                 static_cast<float>(playerPosCoord.z),
                                 ROOM_Z_SCALE);
    return camera;
}

void WeatherSystem::populateWeatherParams(GLRenderState::Uniforms::Weather::Params &params) const
{
    params.intensities = glm::vec4(std::max(m_rainIntensityStart, m_snowIntensityStart),
                                   m_cloudsIntensityStart,
                                   m_fogIntensityStart,
                                   m_precipitationTypeStart);

    params.targets = glm::vec4(std::max(m_targetRainIntensity, m_targetSnowIntensity),
                               m_targetCloudsIntensity,
                               m_targetFogIntensity,
                               m_targetPrecipitationType);

    auto toNamedColorIdx = [](MumeTimeEnum timeOfDay) -> float {
        switch (timeOfDay) {
        case MumeTimeEnum::DAY:
            return static_cast<float>(NamedColorEnum::TRANSPARENT);
        case MumeTimeEnum::NIGHT:
            return static_cast<float>(NamedColorEnum::WEATHER_NIGHT);
        case MumeTimeEnum::DAWN:
            return static_cast<float>(NamedColorEnum::WEATHER_DAWN);
        case MumeTimeEnum::DUSK:
            return static_cast<float>(NamedColorEnum::WEATHER_DUSK);
        case MumeTimeEnum::UNKNOWN:
            return static_cast<float>(NamedColorEnum::TRANSPARENT);
        }
        return static_cast<float>(NamedColorEnum::TRANSPARENT);
    };

    params.timeOfDayIndices.x = toNamedColorIdx(m_oldTimeOfDay);
    params.timeOfDayIndices.y = toNamedColorIdx(m_currentTimeOfDay);
    params.timeOfDayIndices.z = m_timeOfDayIntensityStart;
    params.timeOfDayIndices.w = m_targetTimeOfDayIntensity;

    params.config.x = m_weatherTransitionStartTime;
    params.config.y = m_timeOfDayTransitionStartTime;
    params.config.z = TRANSITION_DURATION;
}

WeatherRenderer::WeatherRenderer(OpenGL &gl,
                                 MapData &mapData,
                                 const MapCanvasTextures &textures,
                                 GameObserver &observer,
                                 AnimationManager &animationManager)
    : m_gl(gl)
    , m_data(mapData)
    , m_textures(textures)
    , m_system(std::make_unique<WeatherSystem>(observer, animationManager))
    , m_animationManager(animationManager)
{
    m_animationManager.registerCallback(m_system->getSignalLifetime(),
                                        [this]() { return m_system->isAnimating(); });

    m_system->sig_stateInvalidated.connect(m_system->getSignalLifetime(), [this]() {
        invalidateWeather();
        sig_requestUpdate.invoke();
    });

    m_gl.getUboManager().registerRebuildFunction(
        Legacy::SharedVboEnum::CameraBlock, [this](Legacy::Functions &glFuncs) {
            const auto playerPosCoord = m_data.tryGetPosition().value_or(Coordinate{0, 0, 0});
            auto cameraData = m_system->getCameraData(m_lastViewProj, playerPosCoord);
            m_gl.getUboManager().update(glFuncs, Legacy::SharedVboEnum::CameraBlock, cameraData);
        });

    m_gl.getUboManager().registerRebuildFunction(
        Legacy::SharedVboEnum::WeatherBlock, [this](Legacy::Functions &glFuncs) {
            GLRenderState::Uniforms::Weather::Params params;
            m_system->populateWeatherParams(params);
            m_gl.getUboManager().update(glFuncs, Legacy::SharedVboEnum::WeatherBlock, params);
        });
}

WeatherRenderer::~WeatherRenderer() = default;

void WeatherRenderer::invalidateCamera()
{
    m_gl.getUboManager().invalidate(Legacy::SharedVboEnum::CameraBlock);
}

void WeatherRenderer::invalidateWeather()
{
    m_gl.getUboManager().invalidate(Legacy::SharedVboEnum::WeatherBlock);
}

void WeatherRenderer::initMeshes()
{
    if (!m_simulation) {
        auto funcs = m_gl.getSharedFunctions(Badge<WeatherRenderer>{});
        auto &shaderPrograms = funcs->getShaderPrograms();

        m_simulation = std::make_unique<Legacy::ParticleSimulationMesh>(
            funcs, shaderPrograms.getParticleSimulationShader());
        m_particles
            = std::make_unique<Legacy::ParticleRenderMesh>(funcs,
                                                           shaderPrograms.getParticleRenderShader(),
                                                           *m_simulation);
        m_atmosphere = UniqueMesh(
            std::make_unique<Legacy::AtmosphereMesh>(funcs, shaderPrograms.getAtmosphereShader()));
        m_timeOfDay = UniqueMesh(
            std::make_unique<Legacy::TimeOfDayMesh>(funcs, shaderPrograms.getTimeOfDayShader()));
    }
}

void WeatherRenderer::update(float /*frameDeltaTime*/)
{
    m_system->update();
}

void WeatherRenderer::prepare(const glm::mat4 &viewProj, const Coordinate &playerPos)
{
    initMeshes();

    if (viewProj != m_lastViewProj || playerPos != m_lastPlayerPos) {
        m_lastViewProj = viewProj;
        m_lastPlayerPos = playerPos;
        invalidateCamera();
    }

    auto &funcs = deref(m_gl.getSharedFunctions(Badge<WeatherRenderer>{}));
    m_gl.getUboManager().bind(funcs, Legacy::SharedVboEnum::CameraBlock);
    m_gl.getUboManager().bind(funcs, Legacy::SharedVboEnum::WeatherBlock);
}

void WeatherRenderer::render(const GLRenderState &rs)
{
    // 1. Render Particles (Simulation + Rendering)
    const float rainMax = m_system->getCurrentRainIntensity();
    const float snowMax = m_system->getCurrentSnowIntensity();
    if (rainMax > 0.0f || snowMax > 0.0f) {
        const auto particleRs = rs.withBlend(BlendModeEnum::MAX_ALPHA);
        if (m_simulation) {
            m_simulation->render(particleRs);
        }
        if (m_particles) {
            m_particles->render(particleRs);
        }
    }

    // 2. Render Atmosphere (TimeOfDay + Atmosphere)
    const auto atmosphereRs = rs.withBlend(BlendModeEnum::TRANSPARENCY)
                                  .withDepthFunction(std::nullopt);

    // TimeOfDay
    if (m_system->getCurrentTimeOfDay() != MumeTimeEnum::DAY
        || m_system->getOldTimeOfDay() != MumeTimeEnum::DAY
        || m_system->getCurrentTimeOfDayIntensity() > 0.0f) {
        if (m_timeOfDay) {
            m_timeOfDay.render(atmosphereRs);
        }
    }

    // Atmosphere
    const float cloudMax = m_system->getCurrentCloudsIntensity();
    const float fogMax = m_system->getCurrentFogIntensity();
    if ((cloudMax > 0.0f || fogMax > 0.0f) && m_atmosphere) {
        m_atmosphere.render(atmosphereRs.withTexture0(m_textures.noise->getId()));
    }
}

QImage WeatherRenderer::generateNoiseTexture(int size)
{
    QImage img(size, size, QImage::Format_RGBA8888);
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            float v = noise(static_cast<float>(x), static_cast<float>(y), size);
            int val = std::clamp(static_cast<int>(v * 255.0f), 0, 255);
            img.setPixelColor(x, y, QColor(val, val, val, 255));
        }
    }
    return img;
}
