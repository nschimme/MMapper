#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "../clock/mumemoment.h"
#include "../global/ChangeMonitor.h"
#include "../global/ConfigEnums.h"
#include "../global/RuleOf5.h"
#include "../map/PromptFlags.h"
#include "../observer/gameobserver.h"
#include "../opengl/OpenGL.h"
#include "../opengl/OpenGLTypes.h"
#include "../opengl/legacy/Legacy.h"
#include "Textures.h"
#include "WeatherRenderables.h"

#include <chrono>
#include <functional>
#include <memory>

class MapData;

class NODISCARD WeatherRenderer final
{
public:
    struct State
    {
        uint32_t currentBuffer = 0;
        uint32_t numParticles = 0;
        bool initialized = false;

        float rainIntensityStart = 0.0f;
        float snowIntensityStart = 0.0f;
        float cloudsIntensityStart = 0.0f;
        float fogIntensityStart = 0.0f;
        float timeOfDayIntensityStart = 0.0f;
        float moonIntensityStart = 0.0f;

        float targetRainIntensity = 0.0f;
        float targetSnowIntensity = 0.0f;
        float targetCloudsIntensity = 0.0f;
        float targetFogIntensity = 0.0f;
        float targetTimeOfDayIntensity = 0.0f;
        float targetMoonIntensity = 0.0f;

        float precipitationTypeStart = 0.0f;
        float targetPrecipitationType = 0.0f;

        float gameRainIntensity = 0.0f;
        float gameSnowIntensity = 0.0f;
        float gameCloudsIntensity = 0.0f;
        float gameFogIntensity = 0.0f;
        float gameTimeOfDayIntensity = 0.0f;

        MumeTimeEnum oldTimeOfDay = MumeTimeEnum::DAY;
        MumeTimeEnum currentTimeOfDay = MumeTimeEnum::DAY;
        MumeMoonVisibilityEnum moonVisibility = MumeMoonVisibilityEnum::UNKNOWN;

        float weatherTransitionStartTime = -2.0f;
        float timeOfDayTransitionStartTime = -2.0f;

        float animationTime = 0.0f;
        float lastDt = 0.0f;
        float lastUboUploadTime = -1.0f;

        std::chrono::steady_clock::time_point lastUpdateTime;
    };

private:
    OpenGL &m_gl;
    MapData &m_data;
    const MapCanvasTextures &m_textures;
    GameObserver &m_observer;
    ChangeMonitor::Lifetime m_lifetime;
    std::function<void(bool)> m_setAnimating;
    State m_state;
    UniqueMesh m_simulation;
    UniqueMesh m_particles;
    UniqueMesh m_atmosphere;
    UniqueMesh m_timeOfDay;
    std::optional<GLRenderState::Uniforms::Weather::Static> m_staticWeather;
    glm::mat4 m_lastViewProj{0.0f};

public:
    explicit WeatherRenderer(OpenGL &gl,
                             MapData &data,
                             const MapCanvasTextures &textures,
                             GameObserver &observer,
                             std::function<void(bool)> setAnimating);
    ~WeatherRenderer();

    DELETE_CTORS_AND_ASSIGN_OPS(WeatherRenderer);

public:
    void init();
    void prepare(const glm::mat4 &viewProj);
    void renderParticles(const glm::mat4 &viewProj);
    void renderAtmosphere(const glm::mat4 &viewProj);
    void update(float dt);

    NODISCARD State &getState() { return m_state; }
    NODISCARD const State &getState() const { return m_state; }

private:
    void updateUbo(const glm::mat4 &viewProj);
    void invalidateStatic();
};
