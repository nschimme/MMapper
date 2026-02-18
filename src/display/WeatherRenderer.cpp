// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "WeatherRenderer.h"

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
            m_state.targetPrecipitationType = 0.0f;
            break;
        case PromptWeatherEnum::HEAVY_RAIN:
            m_state.gameCloudsIntensity = 1.0f;
            m_state.gameRainIntensity = 1.0f;
            m_state.targetPrecipitationType = 0.0f;
            break;
        case PromptWeatherEnum::SNOW:
            m_state.gameCloudsIntensity = 0.8f;
            m_state.gameSnowIntensity = 0.8f;
            m_state.targetPrecipitationType = 1.0f;
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

    m_state.currentTimeOfDay = m_observer.getTimeOfDay();
    m_state.oldTimeOfDay = m_state.currentTimeOfDay;
    m_state.gameTimeOfDayIntensity = (m_state.currentTimeOfDay == MumeTimeEnum::DAY) ? 0.0f : 1.0f;

    m_state.isArtificialLight = m_observer.getArtificialLight();
    m_state.targetArtificialLightIntensity = m_state.isArtificialLight ? 1.0f : 0.0f;

    auto updateTargets = [this, &canvasSettings]() {
        m_state.targetRainIntensity = m_state.gameRainIntensity
                                      * (static_cast<float>(
                                             canvasSettings.weatherPrecipitationIntensity.get())
                                         / 50.0f);
        m_state.targetSnowIntensity = m_state.gameSnowIntensity
                                      * (static_cast<float>(
                                             canvasSettings.weatherPrecipitationIntensity.get())
                                         / 50.0f);
        m_state.targetCloudsIntensity = m_state.gameCloudsIntensity
                                        * (static_cast<float>(
                                               canvasSettings.weatherAtmosphereIntensity.get())
                                           / 50.0f);
        m_state.targetFogIntensity = m_state.gameFogIntensity
                                     * (static_cast<float>(
                                            canvasSettings.weatherAtmosphereIntensity.get())
                                        / 50.0f);
        m_state.targetTimeOfDayIntensity = m_state.gameTimeOfDayIntensity
                                           * (static_cast<float>(
                                                  canvasSettings.weatherTimeOfDayIntensity.get())
                                              / 50.0f);
    };

    updateTargets();

    m_state.timeOfDayIntensityStart = m_state.targetTimeOfDayIntensity;
    m_state.rainIntensityStart = m_state.targetRainIntensity;
    m_state.snowIntensityStart = m_state.targetSnowIntensity;
    m_state.cloudsIntensityStart = m_state.targetCloudsIntensity;
    m_state.fogIntensityStart = m_state.targetFogIntensity;
    m_state.precipitationTypeStart = m_state.targetPrecipitationType;
    m_state.moonIntensityStart = m_state.targetMoonIntensity;
    m_state.artificialLightIntensityStart = m_state.targetArtificialLightIntensity;

    auto lerpCurrentIntensities = [this]() {
        float t = (m_state.animationTime - m_state.weatherTransitionStartTime)
                  / TRANSITION_DURATION;
        float factor = std::clamp(t, 0.0f, 1.0f);
        m_state.rainIntensityStart = my_lerp(m_state.rainIntensityStart,
                                             m_state.targetRainIntensity,
                                             factor);
        m_state.snowIntensityStart = my_lerp(m_state.snowIntensityStart,
                                             m_state.targetSnowIntensity,
                                             factor);
        m_state.cloudsIntensityStart = my_lerp(m_state.cloudsIntensityStart,
                                               m_state.targetCloudsIntensity,
                                               factor);
        m_state.fogIntensityStart = my_lerp(m_state.fogIntensityStart,
                                            m_state.targetFogIntensity,
                                            factor);
        m_state.precipitationTypeStart = my_lerp(m_state.precipitationTypeStart,
                                                 m_state.targetPrecipitationType,
                                                 factor);
    };

    m_observer.sig2_weatherChanged
        .connect(m_lifetime,
                 [this, lerpCurrentIntensities, updateFromGame, updateTargets](PromptWeatherEnum) {
                     lerpCurrentIntensities();
                     updateFromGame();
                     updateTargets();
                     m_state.weatherTransitionStartTime = m_state.animationTime;
                     invalidateStatic();
                     m_setAnimating(true);
                 });

    m_observer.sig2_fogChanged
        .connect(m_lifetime,
                 [this, lerpCurrentIntensities, updateFromGame, updateTargets](PromptFogEnum) {
                     lerpCurrentIntensities();
                     updateFromGame();
                     updateTargets();
                     m_state.weatherTransitionStartTime = m_state.animationTime;
                     invalidateStatic();
                     m_setAnimating(true);
                 });

    m_observer.sig2_timeOfDayChanged
        .connect(m_lifetime, [this, updateTargets](MumeTimeEnum timeOfDay) {
            if (timeOfDay == m_state.currentTimeOfDay) {
                return;
            }

            float t = (m_state.animationTime - m_state.timeOfDayTransitionStartTime)
                      / TRANSITION_DURATION;
            float factor = std::clamp(t, 0.0f, 1.0f);
            m_state.timeOfDayIntensityStart = my_lerp(m_state.timeOfDayIntensityStart,
                                                      m_state.targetTimeOfDayIntensity,
                                                      factor);
            m_state.moonIntensityStart = my_lerp(m_state.moonIntensityStart,
                                                 m_state.targetMoonIntensity,
                                                 factor);

            m_state.oldTimeOfDay = m_state.currentTimeOfDay;
            m_state.currentTimeOfDay = timeOfDay;
            m_state.gameTimeOfDayIntensity = (timeOfDay == MumeTimeEnum::DAY) ? 0.0f : 1.0f;
            updateTargets();
            m_state.timeOfDayTransitionStartTime = m_state.animationTime;
            invalidateStatic();
            m_setAnimating(true);
        });

    m_observer.sig2_moonVisibilityChanged
        .connect(m_lifetime, [this](MumeMoonVisibilityEnum visibility) {
            if (visibility == m_state.moonVisibility) {
                return;
            }
            float t = (m_state.animationTime - m_state.timeOfDayTransitionStartTime)
                      / TRANSITION_DURATION;
            float factor = std::clamp(t, 0.0f, 1.0f);
            m_state.moonIntensityStart = my_lerp(m_state.moonIntensityStart,
                                                 m_state.targetMoonIntensity,
                                                 factor);
            m_state.timeOfDayIntensityStart = my_lerp(m_state.timeOfDayIntensityStart,
                                                      m_state.targetTimeOfDayIntensity,
                                                      factor);

            m_state.moonVisibility = visibility;
            m_state.targetMoonIntensity = (visibility == MumeMoonVisibilityEnum::BRIGHT) ? 1.0f
                                                                                         : 0.0f;
            m_state.timeOfDayTransitionStartTime = m_state.animationTime;
            invalidateStatic();
            m_setAnimating(true);
        });

    m_observer.sig2_artificialLightChanged.connect(m_lifetime, [this](bool isArtificial) {
        if (isArtificial == m_state.isArtificialLight) {
            return;
        }

        float t = (m_state.animationTime - m_state.timeOfDayTransitionStartTime)
                  / TRANSITION_DURATION;
        float factor = std::clamp(t, 0.0f, 1.0f);
        m_state.artificialLightIntensityStart = my_lerp(m_state.artificialLightIntensityStart,
                                                        m_state.targetArtificialLightIntensity,
                                                        factor);

        m_state.isArtificialLight = isArtificial;
        m_state.targetArtificialLightIntensity = isArtificial ? 1.0f : 0.0f;

        m_state.timeOfDayTransitionStartTime = m_state.animationTime;
        invalidateStatic();
        m_setAnimating(true);
    });

    auto onSettingChanged = [this, lerpCurrentIntensities, updateTargets]() {
        lerpCurrentIntensities();
        updateTargets();
        m_state.weatherTransitionStartTime = m_state.animationTime;
        invalidateStatic();
        m_setAnimating(true);
    };

    auto onTimeOfDaySettingChanged = [this, updateTargets]() {
        float t = (m_state.animationTime - m_state.timeOfDayTransitionStartTime)
                  / TRANSITION_DURATION;
        float factor = std::clamp(t, 0.0f, 1.0f);
        m_state.timeOfDayIntensityStart = my_lerp(m_state.timeOfDayIntensityStart,
                                                  m_state.targetTimeOfDayIntensity,
                                                  factor);

        updateTargets();
        m_state.timeOfDayTransitionStartTime = m_state.animationTime;
        invalidateStatic();
        m_setAnimating(true);
    };

    setConfig().canvas.weatherPrecipitationIntensity.registerChangeCallback(m_lifetime,
                                                                            onSettingChanged);
    setConfig().canvas.weatherAtmosphereIntensity.registerChangeCallback(m_lifetime,
                                                                         onSettingChanged);
    setConfig().canvas.weatherTimeOfDayIntensity.registerChangeCallback(m_lifetime,
                                                                        onTimeOfDaySettingChanged);

    m_posConn = QObject::connect(&m_data, &MapData::sig_onPositionChange, [this]() {
        invalidateStatic();
    });
    m_forcedPosConn = QObject::connect(&m_data, &MapData::sig_onForcedPositionChange, [this]() {
        invalidateStatic();
    });
}

WeatherRenderer::~WeatherRenderer()
{
    QObject::disconnect(m_posConn);
    QObject::disconnect(m_forcedPosConn);
}

void WeatherRenderer::invalidateStatic()
{
    m_staticWeather.reset();
}

void WeatherRenderer::init()
{
    if (!m_simulation || !m_particles || !m_atmosphere || !m_timeOfDay) {
        auto funcs = m_gl.getSharedFunctions(Badge<WeatherRenderer>{});
        m_simulation = UniqueMesh(std::make_unique<Legacy::WeatherSimulationMesh>(funcs, *this));
        m_particles = UniqueMesh(std::make_unique<Legacy::WeatherParticleMesh>(funcs, *this));
        m_atmosphere = UniqueMesh(
            std::make_unique<TexturedRenderable>(m_textures.noise->getId(),
                                                 std::make_unique<Legacy::WeatherAtmosphereMesh>(
                                                     funcs)));
        m_timeOfDay = UniqueMesh(std::make_unique<Legacy::WeatherTimeOfDayMesh>(funcs));
    }
}

void WeatherRenderer::update(float dt)
{
    const auto &canvasSettings = getConfig().canvas;
    m_state.targetRainIntensity = m_state.gameRainIntensity
                                  * (static_cast<float>(
                                         canvasSettings.weatherPrecipitationIntensity.get())
                                     / 50.0f);
    m_state.targetSnowIntensity = m_state.gameSnowIntensity
                                  * (static_cast<float>(
                                         canvasSettings.weatherPrecipitationIntensity.get())
                                     / 50.0f);
    m_state.targetCloudsIntensity = m_state.gameCloudsIntensity
                                    * (static_cast<float>(
                                           canvasSettings.weatherAtmosphereIntensity.get())
                                       / 50.0f);
    m_state.targetFogIntensity = m_state.gameFogIntensity
                                 * (static_cast<float>(
                                        canvasSettings.weatherAtmosphereIntensity.get())
                                    / 50.0f);
    m_state.targetTimeOfDayIntensity = m_state.gameTimeOfDayIntensity
                                       * (static_cast<float>(
                                              canvasSettings.weatherTimeOfDayIntensity.get())
                                          / 50.0f);

    m_state.lastDt = dt;
    m_state.animationTime += dt;

    bool weatherTransitioning = (m_state.animationTime - m_state.weatherTransitionStartTime
                                 < TRANSITION_DURATION);
    bool timeOfDayTransitioning = (m_state.animationTime - m_state.timeOfDayTransitionStartTime
                                   < TRANSITION_DURATION);

    if (!weatherTransitioning
        && (std::abs(m_state.rainIntensityStart - m_state.targetRainIntensity) > 1e-5f
            || std::abs(m_state.snowIntensityStart - m_state.targetSnowIntensity) > 1e-5f
            || std::abs(m_state.cloudsIntensityStart - m_state.targetCloudsIntensity) > 1e-5f
            || std::abs(m_state.fogIntensityStart - m_state.targetFogIntensity) > 1e-5f
            || std::abs(m_state.precipitationTypeStart - m_state.targetPrecipitationType) > 1e-5f)) {
        m_state.rainIntensityStart = m_state.targetRainIntensity;
        m_state.snowIntensityStart = m_state.targetSnowIntensity;
        m_state.cloudsIntensityStart = m_state.targetCloudsIntensity;
        m_state.fogIntensityStart = m_state.targetFogIntensity;
        m_state.precipitationTypeStart = m_state.targetPrecipitationType;
        invalidateStatic();
    }

    if (!timeOfDayTransitioning
        && (m_state.oldTimeOfDay != m_state.currentTimeOfDay
            || std::abs(m_state.timeOfDayIntensityStart - m_state.targetTimeOfDayIntensity) > 1e-5f
            || std::abs(m_state.moonIntensityStart - m_state.targetMoonIntensity) > 1e-5f
            || std::abs(m_state.artificialLightIntensityStart
                        - m_state.targetArtificialLightIntensity)
                   > 1e-5f)) {
        m_state.oldTimeOfDay = m_state.currentTimeOfDay;
        m_state.timeOfDayIntensityStart = m_state.targetTimeOfDayIntensity;
        m_state.moonIntensityStart = m_state.targetMoonIntensity;
        m_state.artificialLightIntensityStart = m_state.targetArtificialLightIntensity;
        invalidateStatic();
    }

    bool transitioning = weatherTransitioning || timeOfDayTransitioning;

    // We only need to animate if a transition is active OR weather effects that move (rain, snow, clouds, fog) are present.
    // Static Time of Day tinting does not require continuous animation.
    // Flickering artificial light DOES require continuous animation.
    bool hasActiveWeather = m_state.targetRainIntensity > 0.0f || m_state.targetSnowIntensity > 0.0f
                            || m_state.targetCloudsIntensity > 0.0f
                            || m_state.targetFogIntensity > 0.0f
                            || m_state.rainIntensityStart > 0.0f
                            || m_state.snowIntensityStart > 0.0f
                            || m_state.cloudsIntensityStart > 0.0f
                            || m_state.fogIntensityStart > 0.0f;

    bool hasFlicker = m_state.targetArtificialLightIntensity > 0.0f
                      || m_state.artificialLightIntensityStart > 0.0f;

    m_setAnimating(transitioning || hasActiveWeather || hasFlicker);
}

void WeatherRenderer::prepare(const glm::mat4 &viewProj)
{
    init();
    updateUbo(viewProj);
}

void WeatherRenderer::updateUbo(const glm::mat4 &viewProj)
{
    auto gl_funcs = m_gl.getSharedFunctions(Badge<WeatherRenderer>{});
    auto &funcs = *gl_funcs;

    // 1. Time UBO (Binding 2) - ALWAYS update every frame
    const auto frameBufferEnum = Legacy::SharedVboEnum::TimeBlock;
    const auto vboFrame = funcs.getSharedVbos().get(frameBufferEnum);
    if (!*vboFrame) {
        vboFrame->emplace(gl_funcs);
    }

    GLRenderState::Uniforms::Weather::Frame f;
    f.time = glm::vec2(m_state.animationTime, m_state.lastDt);

    funcs.glBindBuffer(GL_UNIFORM_BUFFER, vboFrame->get());
    funcs.glBufferData(GL_UNIFORM_BUFFER, sizeof(f), &f, GL_DYNAMIC_DRAW);
    funcs.glBindBufferBase(GL_UNIFORM_BUFFER, static_cast<GLuint>(frameBufferEnum), vboFrame->get());

    // 2. Static/Transition UBO (Binding 1) - update ONLY if camera or weather state changes
    if (!m_staticWeather || viewProj != m_lastViewProj) {
        const auto staticBufferEnum = Legacy::SharedVboEnum::WeatherBlock;
        const auto vboStatic = funcs.getSharedVbos().get(staticBufferEnum);
        if (!*vboStatic) {
            vboStatic->emplace(gl_funcs);
        }

        GLRenderState::Uniforms::Weather::Static s;
        s.viewProj = viewProj;

        const auto playerPosCoord = m_data.tryGetPosition().value_or(Coordinate{0, 0, 0});
        s.playerPos = glm::vec4(static_cast<float>(playerPosCoord.x),
                                static_cast<float>(playerPosCoord.y),
                                static_cast<float>(playerPosCoord.z),
                                ROOM_Z_SCALE);

        s.intensities = glm::vec4(std::max(m_state.rainIntensityStart, m_state.snowIntensityStart),
                                  m_state.cloudsIntensityStart,
                                  m_state.fogIntensityStart,
                                  m_state.precipitationTypeStart);

        s.targets = glm::vec4(std::max(m_state.targetRainIntensity, m_state.targetSnowIntensity),
                              m_state.targetCloudsIntensity,
                              m_state.targetFogIntensity,
                              m_state.targetPrecipitationType);

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

        s.timeOfDayIndices.x = toNamedColorIdx(m_state.oldTimeOfDay);
        s.timeOfDayIndices.y = toNamedColorIdx(m_state.currentTimeOfDay);
        s.timeOfDayIndices.z = m_state.timeOfDayIntensityStart;
        s.timeOfDayIndices.w = m_state.targetTimeOfDayIntensity;

        s.config.x = m_state.weatherTransitionStartTime;
        s.config.y = m_state.timeOfDayTransitionStartTime;
        s.config.z = TRANSITION_DURATION;
        s.config.w = 0.0f;

        s.torch.x = m_state.artificialLightIntensityStart;
        s.torch.y = m_state.targetArtificialLightIntensity;
        s.torch.z = static_cast<float>(NamedColorEnum::WEATHER_TORCH);

        const auto room = m_data.findRoomHandle(playerPosCoord);
        const bool isRoomDark = (room.exists() && room.getLightType() == RoomLightEnum::DARK);
        s.torch.w = isRoomDark ? 1.0f : 0.0f;

        funcs.glBindBuffer(GL_UNIFORM_BUFFER, vboStatic->get());
        funcs.glBufferData(GL_UNIFORM_BUFFER, sizeof(s), &s, GL_DYNAMIC_DRAW);
        funcs.glBindBufferBase(GL_UNIFORM_BUFFER,
                               static_cast<GLuint>(staticBufferEnum),
                               vboStatic->get());

        m_staticWeather = s;
        m_lastViewProj = viewProj;
    }

    m_state.lastUboUploadTime = m_state.animationTime;
}

void WeatherRenderer::renderParticles(const glm::mat4 & /*viewProj*/)
{
    const float rainMax = std::max(m_state.rainIntensityStart, m_state.targetRainIntensity);
    const float snowMax = std::max(m_state.snowIntensityStart, m_state.targetSnowIntensity);

    if (rainMax <= 0.0f && snowMax <= 0.0f) {
        return;
    }

    const auto rs = m_gl.getDefaultRenderState().withBlend(BlendModeEnum::MAX_ALPHA);

    m_simulation.render(rs);
    m_particles.render(rs);
}

void WeatherRenderer::renderAtmosphere(const glm::mat4 & /*viewProj*/)
{
    init();

    const auto rs = m_gl.getDefaultRenderState()
                        .withBlend(BlendModeEnum::TRANSPARENCY)
                        .withDepthFunction(std::nullopt);

    // 1. Render TimeOfDay Overlay (Full Screen)
    if (m_state.currentTimeOfDay != MumeTimeEnum::DAY || m_state.oldTimeOfDay != MumeTimeEnum::DAY
        || m_state.timeOfDayIntensityStart > 0.0f || m_state.targetTimeOfDayIntensity > 0.0f) {
        m_timeOfDay.render(rs);
    }

    // 2. Render Atmosphere Overlay (Fog/Clouds)
    const float cloudMax = std::max(m_state.cloudsIntensityStart, m_state.targetCloudsIntensity);
    const float fogMax = std::max(m_state.fogIntensityStart, m_state.targetFogIntensity);

    if (cloudMax > 0.0f || fogMax > 0.0f) {
        m_atmosphere.render(rs);
    }
}
