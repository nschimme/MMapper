// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "Weather.h"

#include "../configuration/configuration.h"
#include "../display/FrameManager.h"
#include "../display/Textures.h"
#include "../global/Badge.h"
#include "../map/coordinate.h"
#include "../mapdata/mapdata.h"
#include "OpenGL.h"
#include "OpenGLTypes.h"
#include "legacy/Legacy.h"

#include <algorithm>
#include <cstdlib>

#include <glm/glm.hpp>

namespace {
constexpr float ROOM_Z_SCALE = 7.f;

template<typename T>
T lerp(T a, T b, float t)
{
    return a + t * (b - a);
}

} // namespace

/**
 * Weather transition authority documentation:
 *
 * Transitions (weather intensities, fog, time-of-day) are driven by a shared
 * duration constant (WeatherConstants::TRANSITION_DURATION).
 *
 * The CPU (GLWeather::update/applyTransition) calculates interpolated values
 * each frame. These CPU-side values are primarily used for high-level logic,
 * such as:
 * - Pruning: Skipping rendering of atmosphere/particles when intensities are zero.
 * - Thinning: Scaling the number of particle instances based on intensity for performance.
 * - Pacing: Determining if FrameManager heartbeat should continue.
 *
 * The GPU (shaders) is authoritative for the visual transition curves.
 * By passing start/target pairs and start times to the shaders, the GPU
 * performs per-pixel (or per-vertex) interpolation using its own clocks.
 * This ensures perfectly smooth visuals even if the CPU frame rate is low
 * or inconsistent, while avoiding constant UBO re-uploads for animation state.
 */

GLWeather::GLWeather(OpenGL &gl,
                     MapData &mapData,
                     const MapCanvasTextures &textures,
                     GameObserver &observer,
                     FrameManager &animationManager)
    : m_gl(gl)
    , m_data(mapData)
    , m_textures(textures)
    , m_animationManager(animationManager)
    , m_observer(observer)
{
    updateFromGame();

    m_moonVisibility = m_observer.getMoonVisibility();
    m_currentTimeOfDay = m_observer.getTimeOfDay();
    m_gameTimeOfDayIntensity = (m_currentTimeOfDay == MumeTimeEnum::DAY) ? 0.0f : 1.0f;

    updateTargets();

    m_startColorIdx = m_targetColorIdx = getCurrentColorIdx();

    m_timeOfDayIntensityStart = m_targetTimeOfDayIntensity;
    m_rainIntensityStart = m_targetRainIntensity;
    m_snowIntensityStart = m_targetSnowIntensity;
    m_cloudsIntensityStart = m_targetCloudsIntensity;
    m_fogIntensityStart = m_targetFogIntensity;
    m_precipitationTypeStart = m_targetPrecipitationType;

    auto startWeatherTransitions = [this]() {
        const bool startingFromNice = (m_currentRainIntensity <= 0.0f
                                       && m_currentSnowIntensity <= 0.0f);

        startTransitions(m_weatherTransitionStartTime,
                         TransitionPair<float>{m_rainIntensityStart, m_targetRainIntensity},
                         TransitionPair<float>{m_snowIntensityStart, m_targetSnowIntensity},
                         TransitionPair<float>{m_cloudsIntensityStart, m_targetCloudsIntensity},
                         TransitionPair<float>{m_fogIntensityStart, m_targetFogIntensity},
                         TransitionPair<float>{m_precipitationTypeStart, m_targetPrecipitationType});

        if (startingFromNice) {
            // If we are starting from clear skies, snap the precipitation type to the target
            // immediately so we don't see a mix (e.g. rain turning into snow) during the fade-in.
            m_precipitationTypeStart = m_targetPrecipitationType;
        }
    };

    m_observer.sig2_weatherChanged.connect(m_lifetime,
                                           [this, startWeatherTransitions](PromptWeatherEnum) {
                                               startWeatherTransitions();
                                               updateFromGame();
                                               updateTargets();
                                               invalidateWeather();
                                               m_animationManager.requestUpdate();
                                           });

    m_observer.sig2_fogChanged.connect(m_lifetime, [this, startWeatherTransitions](PromptFogEnum) {
        startWeatherTransitions();
        updateFromGame();
        updateTargets();
        invalidateWeather();
        m_animationManager.requestUpdate();
    });

    auto startTimeOfDayTransitions = [this]() {
        startTransitions(m_timeOfDayTransitionStartTime,
                         TransitionPair<float>{m_timeOfDayIntensityStart,
                                               m_targetTimeOfDayIntensity},
                         TransitionPair<NamedColorEnum>{m_startColorIdx, m_targetColorIdx});
    };

    m_observer.sig2_timeOfDayChanged
        .connect(m_lifetime, [this, startTimeOfDayTransitions](MumeTimeEnum timeOfDay) {
            if (timeOfDay == m_currentTimeOfDay) {
                return;
            }

            startTimeOfDayTransitions();

            m_currentTimeOfDay = timeOfDay;
            m_gameTimeOfDayIntensity = (timeOfDay == MumeTimeEnum::DAY) ? 0.0f : 1.0f;
            updateTargets();
            m_targetColorIdx = getCurrentColorIdx();

            invalidateWeather();
            m_animationManager.requestUpdate();
        });

    m_observer.sig2_moonVisibilityChanged.connect(m_lifetime,
                                                  [this, startTimeOfDayTransitions](
                                                      MumeMoonVisibilityEnum visibility) {
                                                      if (visibility == m_moonVisibility) {
                                                          return;
                                                      }
                                                      startTimeOfDayTransitions();

                                                      m_moonVisibility = visibility;
                                                      m_targetColorIdx = getCurrentColorIdx();

                                                      invalidateWeather();
                                                      m_animationManager.requestUpdate();
                                                  });

    auto onSettingChanged = [this, startWeatherTransitions]() {
        startWeatherTransitions();
        updateTargets();
        invalidateWeather();
        m_animationManager.requestUpdate();
    };

    auto onTimeOfDaySettingChanged = [this, startTimeOfDayTransitions]() {
        startTimeOfDayTransitions();

        updateTargets();
        invalidateWeather();
        m_animationManager.requestUpdate();
    };

    setConfig().canvas.weatherPrecipitationIntensity.registerChangeCallback(m_lifetime,
                                                                            onSettingChanged);
    setConfig().canvas.weatherAtmosphereIntensity.registerChangeCallback(m_lifetime,
                                                                         onSettingChanged);
    setConfig().canvas.weatherTimeOfDayIntensity.registerChangeCallback(m_lifetime,
                                                                        onTimeOfDaySettingChanged);

    m_animationManager.registerCallback(m_lifetime, [this]() {
        return isAnimating() ? FrameManager::AnimationStatusEnum::Continue
                             : FrameManager::AnimationStatusEnum::Stop;
    });

    m_gl.getUboManager().registerRebuildFunction(
        Legacy::SharedVboEnum::CameraBlock, [this](Legacy::Functions &glFuncs) {
            const auto playerPosCoord = m_data.tryGetPosition().value_or(Coordinate{0, 0, 0});
            auto cameraData = getCameraData(glFuncs.getProjectionMatrix(), playerPosCoord);
            m_gl.getUboManager().update(glFuncs, Legacy::SharedVboEnum::CameraBlock, cameraData);
        });

    m_gl.getUboManager().registerRebuildFunction(
        Legacy::SharedVboEnum::WeatherBlock, [this](Legacy::Functions &glFuncs) {
            GLRenderState::Uniforms::Weather::Params params;
            populateWeatherParams(params);
            m_gl.getUboManager().update(glFuncs, Legacy::SharedVboEnum::WeatherBlock, params);
        });
}

GLWeather::~GLWeather()
{
    m_gl.getUboManager().unregisterRebuildFunction(Legacy::SharedVboEnum::CameraBlock);
    m_gl.getUboManager().unregisterRebuildFunction(Legacy::SharedVboEnum::WeatherBlock);
}

void GLWeather::updateFromGame()
{
    auto w = m_observer.getWeather();
    auto f = m_observer.getFog();

    m_gameRainIntensity = 0.0f;
    m_gameSnowIntensity = 0.0f;
    m_gameCloudsIntensity = 0.0f;
    m_gameFogIntensity = 0.0f;

    switch (w) {
    case PromptWeatherEnum::NICE:
        // Preserve m_gamePrecipitationType so it stays at its last value while fading out.
        break;
    case PromptWeatherEnum::CLOUDS:
        m_gameCloudsIntensity = 0.5f;
        // Also preserve type here, as there's no precipitation.
        break;
    case PromptWeatherEnum::RAIN:
        m_gameCloudsIntensity = 0.8f;
        m_gameRainIntensity = 0.5f;
        m_gamePrecipitationType = 0.0f;
        break;
    case PromptWeatherEnum::HEAVY_RAIN:
        m_gameCloudsIntensity = 1.0f;
        m_gameRainIntensity = 1.0f;
        m_gamePrecipitationType = 0.0f;
        break;
    case PromptWeatherEnum::SNOW:
        m_gameCloudsIntensity = 0.8f;
        m_gameSnowIntensity = 0.8f;
        m_gamePrecipitationType = 1.0f;
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

void GLWeather::updateTargets()
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
    m_targetPrecipitationType = m_gamePrecipitationType;
}

void GLWeather::update()
{
    m_currentRainIntensity = applyTransition(m_weatherTransitionStartTime,
                                             m_rainIntensityStart,
                                             m_targetRainIntensity);
    m_currentSnowIntensity = applyTransition(m_weatherTransitionStartTime,
                                             m_snowIntensityStart,
                                             m_targetSnowIntensity);
    m_currentCloudsIntensity = applyTransition(m_weatherTransitionStartTime,
                                               m_cloudsIntensityStart,
                                               m_targetCloudsIntensity);
    m_currentFogIntensity = applyTransition(m_weatherTransitionStartTime,
                                            m_fogIntensityStart,
                                            m_targetFogIntensity);

    m_currentTimeOfDayIntensity = applyTransition(m_timeOfDayTransitionStartTime,
                                                  m_timeOfDayIntensityStart,
                                                  m_targetTimeOfDayIntensity);
}

bool GLWeather::isAnimating() const
{
    const bool activePrecipitation = m_currentRainIntensity > 0.0f || m_currentSnowIntensity > 0.0f;
    const bool activeAtmosphere = m_currentCloudsIntensity > 0.0f || m_currentFogIntensity > 0.0f;
    return isTransitioning() || activePrecipitation || activeAtmosphere;
}

bool GLWeather::isTransitioning() const
{
    const float animTime = m_animationManager.getElapsedTime();
    const bool weatherTransitioning = (animTime - m_weatherTransitionStartTime
                                       < WeatherConstants::TRANSITION_DURATION);
    const bool timeOfDayTransitioning = (animTime - m_timeOfDayTransitionStartTime
                                         < WeatherConstants::TRANSITION_DURATION);
    return weatherTransitioning || timeOfDayTransitioning;
}

GLRenderState::Uniforms::Weather::Camera GLWeather::getCameraData(
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

NamedColorEnum GLWeather::getCurrentColorIdx() const
{
    switch (m_currentTimeOfDay) {
    case MumeTimeEnum::DAY:
        return NamedColorEnum::TRANSPARENT;
    case MumeTimeEnum::NIGHT:
        return (m_moonVisibility == MumeMoonVisibilityEnum::BRIGHT)
                   ? NamedColorEnum::WEATHER_NIGHT_MOON
                   : NamedColorEnum::WEATHER_NIGHT;
    case MumeTimeEnum::DAWN:
        return NamedColorEnum::WEATHER_DAWN;
    case MumeTimeEnum::DUSK:
        return NamedColorEnum::WEATHER_DUSK;
    case MumeTimeEnum::UNKNOWN:
        return NamedColorEnum::TRANSPARENT;
    }
    return NamedColorEnum::TRANSPARENT;
}

void GLWeather::populateWeatherParams(GLRenderState::Uniforms::Weather::Params &params) const
{
    params.intensities = glm::vec4(std::max(m_rainIntensityStart, m_snowIntensityStart),
                                   m_cloudsIntensityStart,
                                   m_fogIntensityStart,
                                   m_precipitationTypeStart);

    params.targets = glm::vec4(std::max(m_targetRainIntensity, m_targetSnowIntensity),
                               m_targetCloudsIntensity,
                               m_targetFogIntensity,
                               m_targetPrecipitationType);

    params.timeOfDayIndices.x = static_cast<float>(m_startColorIdx);
    params.timeOfDayIndices.y = static_cast<float>(m_targetColorIdx);
    params.timeOfDayIndices.z = m_timeOfDayIntensityStart;
    params.timeOfDayIndices.w = m_targetTimeOfDayIntensity;

    params.config.x = m_weatherTransitionStartTime;
    params.config.y = m_timeOfDayTransitionStartTime;
    params.config.z = WeatherConstants::TRANSITION_DURATION;
}

void GLWeather::invalidateWeather()
{
    m_gl.getUboManager().invalidate(Legacy::SharedVboEnum::WeatherBlock);
}

float GLWeather::applyTransition(const float startTime,
                                 const float startVal,
                                 const float targetVal) const
{
    const float t = (m_animationManager.getElapsedTime() - startTime)
                    / WeatherConstants::TRANSITION_DURATION;
    const float factor = std::clamp(t, 0.0f, 1.0f);
    return lerp(startVal, targetVal, factor);
}

template<typename T>
T GLWeather::applyTransition(float startTime, T startVal, T targetVal) const
{
    if (startVal == targetVal) {
        return startVal;
    }
    const float t = (m_animationManager.getElapsedTime() - startTime)
                    / WeatherConstants::TRANSITION_DURATION;
    return (t >= 1.0f) ? targetVal : startVal;
}

template<typename... Pairs>
void GLWeather::startTransitions(float &startTime, Pairs... pairs)
{
    float oldStartTime = startTime;
    startTime = m_animationManager.getElapsedTime();
    (..., (pairs.start = applyTransition(oldStartTime, pairs.start, pairs.target)));
}

template void GLWeather::startTransitions(float &startTime,
                                          TransitionPair<float> p1,
                                          TransitionPair<float> p2,
                                          TransitionPair<float> p3,
                                          TransitionPair<float> p4,
                                          TransitionPair<float> p5);

template void GLWeather::startTransitions(float &startTime,
                                          TransitionPair<float> p1,
                                          TransitionPair<NamedColorEnum> p2);

void GLWeather::initMeshes()
{
    if (!m_simulation) {
        auto funcs = m_gl.getSharedFunctions(Badge<GLWeather>{});
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

void GLWeather::prepare()
{
    initMeshes();

    auto &funcs = deref(m_gl.getSharedFunctions(Badge<GLWeather>{}));
    m_gl.getUboManager().bind(funcs, Legacy::SharedVboEnum::CameraBlock);
    m_gl.getUboManager().bind(funcs, Legacy::SharedVboEnum::WeatherBlock);
}

void GLWeather::render(const GLRenderState &rs)
{
    // 1. Render Particles (Simulation + Rendering)
    const float rainMax = m_currentRainIntensity;
    const float snowMax = m_currentSnowIntensity;
    if (rainMax > 0.0f || snowMax > 0.0f) {
        auto particleRs = rs.withBlend(BlendModeEnum::MAX_ALPHA);
        particleRs.uniforms.weather.currentPrecipitationIntensity = std::max(rainMax, snowMax);

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
    if (m_currentTimeOfDay != MumeTimeEnum::DAY || m_startColorIdx != NamedColorEnum::TRANSPARENT
        || m_currentTimeOfDayIntensity > 0.0f) {
        if (m_timeOfDay) {
            m_timeOfDay.render(atmosphereRs);
        }
    }

    // Atmosphere
    const float cloudMax = m_currentCloudsIntensity;
    const float fogMax = m_currentFogIntensity;
    if ((cloudMax > 0.0f || fogMax > 0.0f) && m_atmosphere) {
        m_atmosphere.render(atmosphereRs.withTexture0(m_textures.noise->getId()));
    }
}
