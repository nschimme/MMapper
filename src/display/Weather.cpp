// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

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

// TODO: dedupe with my_lerp
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

    // Quintic interpolation curve: 6t^5 - 15t^4 + 10t^3
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

WeatherRenderer::WeatherRenderer(OpenGL &gl,
                                 MapData &data,
                                 const MapCanvasTextures &textures,
                                 GameObserver &observer,
                                 AnimationManager &animationManager)
    : m_gl(gl)
    , m_data(data)
    , m_textures(textures)
    , m_observer(observer)
    , m_animationManager(animationManager)
{
    m_animationManager.registerCallback(m_lifetime, [this]() { return isAnimating(); });

    m_gl.getUboManager()
        .registerUbo(Legacy::SharedVboEnum::WeatherBlock, [this](Legacy::Functions &gl_funcs) {
            if (!m_staticUboData) {
                return;
            }
            const auto shared = gl_funcs.getSharedVbos().get(Legacy::SharedVboEnum::WeatherBlock);
            std::ignore = gl_funcs.setUbo(shared->get(),
                                          std::vector<GLRenderState::Uniforms::Weather::Static>{
                                              *m_staticUboData},
                                          BufferUsageEnum::DYNAMIC_DRAW);
        });

    const auto &canvasSettings = getConfig().canvas;

    auto updateFromGame = [this]() {
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
    };

    updateFromGame();

    m_moonVisibility = m_observer.getMoonVisibility();
    m_targetMoonIntensity = (m_moonVisibility == MumeMoonVisibilityEnum::BRIGHT) ? 1.0f : 0.0f;

    m_currentTimeOfDay = m_observer.getTimeOfDay();
    m_oldTimeOfDay = m_currentTimeOfDay;
    m_gameTimeOfDayIntensity = (m_currentTimeOfDay == MumeTimeEnum::DAY) ? 0.0f : 1.0f;

    auto updateTargets = [this, &canvasSettings]() {
        m_targetRainIntensity = m_gameRainIntensity
                                * (static_cast<float>(
                                       canvasSettings.weatherPrecipitationIntensity.get())
                                   / 50.0f);
        m_targetSnowIntensity = m_gameSnowIntensity
                                * (static_cast<float>(
                                       canvasSettings.weatherPrecipitationIntensity.get())
                                   / 50.0f);
        m_targetCloudsIntensity = m_gameCloudsIntensity
                                  * (static_cast<float>(
                                         canvasSettings.weatherAtmosphereIntensity.get())
                                     / 50.0f);
        m_targetFogIntensity = m_gameFogIntensity
                               * (static_cast<float>(canvasSettings.weatherAtmosphereIntensity.get())
                                  / 50.0f);
        m_targetTimeOfDayIntensity = m_gameTimeOfDayIntensity
                                     * (static_cast<float>(
                                            canvasSettings.weatherTimeOfDayIntensity.get())
                                        / 50.0f);
    };

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
        .connect(m_lifetime,
                 [this, lerpCurrentIntensities, updateFromGame, updateTargets](PromptWeatherEnum) {
                     lerpCurrentIntensities();
                     updateFromGame();
                     updateTargets();
                     m_weatherTransitionStartTime = m_animationManager.getAnimationTime();
                     invalidateStatic();
                     sig_requestUpdate.invoke();
                 });

    m_observer.sig2_fogChanged
        .connect(m_lifetime,
                 [this, lerpCurrentIntensities, updateFromGame, updateTargets](PromptFogEnum) {
                     lerpCurrentIntensities();
                     updateFromGame();
                     updateTargets();
                     m_weatherTransitionStartTime = m_animationManager.getAnimationTime();
                     invalidateStatic();
                     sig_requestUpdate.invoke();
                 });

    m_observer.sig2_timeOfDayChanged
        .connect(m_lifetime, [this, updateTargets](MumeTimeEnum timeOfDay) {
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
            invalidateStatic();
            sig_requestUpdate.invoke();
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
            invalidateStatic();
            sig_requestUpdate.invoke();
        });

    auto onSettingChanged = [this, lerpCurrentIntensities, updateTargets]() {
        lerpCurrentIntensities();
        updateTargets();
        m_weatherTransitionStartTime = m_animationManager.getAnimationTime();
        invalidateStatic();
        sig_requestUpdate.invoke();
    };

    auto onTimeOfDaySettingChanged = [this, updateTargets]() {
        float t = (m_animationManager.getAnimationTime() - m_timeOfDayTransitionStartTime)
                  / TRANSITION_DURATION;
        float factor = std::clamp(t, 0.0f, 1.0f);
        m_timeOfDayIntensityStart = my_lerp(m_timeOfDayIntensityStart,
                                            m_targetTimeOfDayIntensity,
                                            factor);

        updateTargets();
        m_timeOfDayTransitionStartTime = m_animationManager.getAnimationTime();
        invalidateStatic();
        sig_requestUpdate.invoke();
    };

    setConfig().canvas.weatherPrecipitationIntensity.registerChangeCallback(m_lifetime,
                                                                            onSettingChanged);
    setConfig().canvas.weatherAtmosphereIntensity.registerChangeCallback(m_lifetime,
                                                                         onSettingChanged);
    setConfig().canvas.weatherTimeOfDayIntensity.registerChangeCallback(m_lifetime,
                                                                        onTimeOfDaySettingChanged);

    m_posConn = QObject::connect(&m_data, &MapData::sig_onPositionChange, [this]() {
        invalidateStatic();
        sig_requestUpdate.invoke();
    });
    m_forcedPosConn = QObject::connect(&m_data, &MapData::sig_onForcedPositionChange, [this]() {
        invalidateStatic();
        sig_requestUpdate.invoke();
    });
}

WeatherRenderer::~WeatherRenderer()
{
    QObject::disconnect(m_posConn);
    QObject::disconnect(m_forcedPosConn);
}

void WeatherRenderer::invalidateStatic()
{
    m_staticUboData.reset();
    m_gl.getUboManager().invalidate(Legacy::SharedVboEnum::WeatherBlock);
}

void WeatherRenderer::initMeshes()
{
    if (!m_simulation) {
        auto funcs = m_gl.getSharedFunctions(Badge<WeatherRenderer>{});
        auto &shaderPrograms = funcs->getShaderPrograms();

        m_simulation = std::make_unique<Legacy::WeatherSimulationMesh>(
            funcs, shaderPrograms.getParticleSimulationShader());
        m_particles = std::make_unique<Legacy::WeatherParticleMesh>(funcs,
                                                                    shaderPrograms
                                                                        .getParticleRenderShader(),
                                                                    *m_simulation);
        m_atmosphere = UniqueMesh(
            std::make_unique<Legacy::WeatherAtmosphereMesh>(funcs,
                                                            shaderPrograms.getAtmosphereShader(),
                                                            m_textures.noise));
        m_timeOfDay = UniqueMesh(
            std::make_unique<Legacy::WeatherTimeOfDayMesh>(funcs,
                                                           shaderPrograms.getTimeOfDayShader()));
    }
}

void WeatherRenderer::update(float /*dt*/)
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

    const float animTime = m_animationManager.getAnimationTime();

    bool weatherTransitioning = (animTime - m_weatherTransitionStartTime < TRANSITION_DURATION);
    bool timeOfDayTransitioning = (animTime - m_timeOfDayTransitionStartTime < TRANSITION_DURATION);

    if (!weatherTransitioning
        && (std::abs(m_rainIntensityStart - m_targetRainIntensity) > 1e-5f
            || std::abs(m_snowIntensityStart - m_targetSnowIntensity) > 1e-5f
            || std::abs(m_cloudsIntensityStart - m_targetCloudsIntensity) > 1e-5f
            || std::abs(m_fogIntensityStart - m_targetFogIntensity) > 1e-5f
            || std::abs(m_precipitationTypeStart - m_targetPrecipitationType) > 1e-5f)) {
        m_rainIntensityStart = m_targetRainIntensity;
        m_snowIntensityStart = m_targetSnowIntensity;
        m_cloudsIntensityStart = m_targetCloudsIntensity;
        m_fogIntensityStart = m_targetFogIntensity;
        m_precipitationTypeStart = m_targetPrecipitationType;
        invalidateStatic();
    }

    if (!timeOfDayTransitioning
        && (m_oldTimeOfDay != m_currentTimeOfDay
            || std::abs(m_timeOfDayIntensityStart - m_targetTimeOfDayIntensity) > 1e-5f
            || std::abs(m_moonIntensityStart - m_targetMoonIntensity) > 1e-5f)) {
        m_oldTimeOfDay = m_currentTimeOfDay;
        m_timeOfDayIntensityStart = m_targetTimeOfDayIntensity;
        m_moonIntensityStart = m_targetMoonIntensity;
        invalidateStatic();
    }

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

bool WeatherRenderer::isAnimating() const
{
    const float animTime = m_animationManager.getAnimationTime();
    bool weatherTransitioning = (animTime - m_weatherTransitionStartTime < TRANSITION_DURATION);
    bool timeOfDayTransitioning = (animTime - m_timeOfDayTransitionStartTime < TRANSITION_DURATION);
    bool transitioning = weatherTransitioning || timeOfDayTransitioning;

    bool hasActiveWeather = m_currentRainIntensity > 0.0f || m_currentSnowIntensity > 0.0f
                            || m_currentCloudsIntensity > 0.0f || m_currentFogIntensity > 0.0f;

    return transitioning || hasActiveWeather;
}

void WeatherRenderer::prepare(const glm::mat4 &viewProj)
{
    initMeshes();

    if (!m_staticUboData || viewProj != m_lastViewProj) {
        GLRenderState::Uniforms::Weather::Static ubo;
        ubo.viewProj = viewProj;

        const auto playerPosCoord = m_data.tryGetPosition().value_or(Coordinate{0, 0, 0});
        ubo.playerPos = glm::vec4(static_cast<float>(playerPosCoord.x),
                                  static_cast<float>(playerPosCoord.y),
                                  static_cast<float>(playerPosCoord.z),
                                  ROOM_Z_SCALE);

        ubo.intensities = glm::vec4(std::max(m_rainIntensityStart, m_snowIntensityStart),
                                    m_cloudsIntensityStart,
                                    m_fogIntensityStart,
                                    m_precipitationTypeStart);

        ubo.targets = glm::vec4(std::max(m_targetRainIntensity, m_targetSnowIntensity),
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

        ubo.timeOfDayIndices.x = toNamedColorIdx(m_oldTimeOfDay);
        ubo.timeOfDayIndices.y = toNamedColorIdx(m_currentTimeOfDay);
        ubo.timeOfDayIndices.z = m_timeOfDayIntensityStart;
        ubo.timeOfDayIndices.w = m_targetTimeOfDayIntensity;

        ubo.config.x = m_weatherTransitionStartTime;
        ubo.config.y = m_timeOfDayTransitionStartTime;
        ubo.config.z = TRANSITION_DURATION;

        m_staticUboData = ubo;
        m_lastViewProj = viewProj;
        m_gl.getUboManager().invalidate(Legacy::SharedVboEnum::WeatherBlock);
    }

    auto &funcs = deref(m_gl.getSharedFunctions(Badge<WeatherRenderer>{}));
    m_gl.getUboManager().updateAndBind(funcs, Legacy::SharedVboEnum::WeatherBlock);

    m_lastUboUploadTime = m_animationManager.getAnimationTime();
}

void WeatherRenderer::render(const GLRenderState &rs)
{
    // 1. Render Particles (Simulation + Rendering)
    const float rainMax = std::max(m_currentRainIntensity, m_targetRainIntensity);
    const float snowMax = std::max(m_currentSnowIntensity, m_targetSnowIntensity);
    if (rainMax > 0.0f || snowMax > 0.0f) {
        const auto particleRs = rs.withBlend(BlendModeEnum::MAX_ALPHA);
        if (m_simulation) {
            m_simulation->render(particleRs);
        }
        if (m_particles) {
            m_particles->render(particleRs, m_currentRainIntensity, m_currentSnowIntensity);
        }
    }

    // 2. Render Atmosphere (TimeOfDay + Atmosphere)
    const auto atmosphereRs = rs.withBlend(BlendModeEnum::TRANSPARENCY)
                                  .withDepthFunction(std::nullopt);

    // TimeOfDay
    if (m_currentTimeOfDay != MumeTimeEnum::DAY || m_oldTimeOfDay != MumeTimeEnum::DAY
        || m_timeOfDayIntensityStart > 0.0f || m_targetTimeOfDayIntensity > 0.0f) {
        if (m_timeOfDay) {
            m_timeOfDay.render(atmosphereRs);
        }
    }

    // Atmosphere
    const float cloudMax = std::max(m_currentCloudsIntensity, m_targetCloudsIntensity);
    const float fogMax = std::max(m_currentFogIntensity, m_targetFogIntensity);
    if ((cloudMax > 0.0f || fogMax > 0.0f) && m_atmosphere) {
        m_atmosphere.render(atmosphereRs);
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
