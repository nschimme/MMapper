#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "../clock/mumemoment.h"
#include "../global/ChangeMonitor.h"
#include "../global/ConfigEnums.h"
#include "../global/RuleOf5.h"
#include "../map/PromptFlags.h"
#include "../map/coordinate.h"
#include "../observer/gameobserver.h"
#include "OpenGL.h"
#include "OpenGLTypes.h"
#include "legacy/Legacy.h"
#include "legacy/WeatherMeshes.h"

#include <chrono>
#include <memory>

class AnimationManager;
class MapData;
struct MapCanvasTextures;

/**
 * @brief Manages the logic and rendering of the weather system.
 * Follows the design and structure of GLFont.
 */
class NODISCARD GLWeather final
{
private:
    OpenGL &m_gl;
    MapData &m_data;
    const MapCanvasTextures &m_textures;
    AnimationManager &m_animationManager;
    GameObserver &m_observer;
    ChangeMonitor::Lifetime m_lifetime;

    // Weather State
    float m_rainIntensityStart = 0.0f;
    float m_snowIntensityStart = 0.0f;
    float m_cloudsIntensityStart = 0.0f;
    float m_fogIntensityStart = 0.0f;
    float m_timeOfDayIntensityStart = 0.0f;
    float m_moonIntensityStart = 0.0f;

    float m_currentRainIntensity = 0.0f;
    float m_currentSnowIntensity = 0.0f;
    float m_currentCloudsIntensity = 0.0f;
    float m_currentFogIntensity = 0.0f;
    float m_currentTimeOfDayIntensity = 0.0f;

    float m_targetRainIntensity = 0.0f;
    float m_targetSnowIntensity = 0.0f;
    float m_targetCloudsIntensity = 0.0f;
    float m_targetFogIntensity = 0.0f;
    float m_targetTimeOfDayIntensity = 0.0f;
    float m_targetMoonIntensity = 0.0f;

    float m_precipitationTypeStart = 0.0f;
    float m_targetPrecipitationType = 0.0f;

    float m_gameRainIntensity = 0.0f;
    float m_gameSnowIntensity = 0.0f;
    float m_gameCloudsIntensity = 0.0f;
    float m_gameFogIntensity = 0.0f;
    float m_gameTimeOfDayIntensity = 0.0f;

    MumeTimeEnum m_oldTimeOfDay = MumeTimeEnum::DAY;
    MumeTimeEnum m_currentTimeOfDay = MumeTimeEnum::DAY;
    MumeMoonVisibilityEnum m_moonVisibility = MumeMoonVisibilityEnum::UNKNOWN;

    float m_weatherTransitionStartTime = -2.0f;
    float m_timeOfDayTransitionStartTime = -2.0f;

    Signal2Lifetime m_signalLifetime;

    // Rendering State
    glm::mat4 m_lastViewProj{0.0f};
    Coordinate m_lastPlayerPos;

    // Meshes
    std::unique_ptr<Legacy::ParticleSimulationMesh> m_simulation;
    std::unique_ptr<Legacy::ParticleRenderMesh> m_particles;
    UniqueMesh m_atmosphere;
    UniqueMesh m_timeOfDay;

public:
    Signal2<> sig_requestUpdate;

public:
    explicit GLWeather(OpenGL &gl,
                       MapData &mapData,
                       const MapCanvasTextures &textures,
                       GameObserver &observer,
                       AnimationManager &animationManager);
    ~GLWeather();

    DELETE_CTORS_AND_ASSIGN_OPS(GLWeather);

public:
    void update();
    void prepare(const glm::mat4 &viewProj, const Coordinate &playerPos);
    void render(const GLRenderState &rs);

    NODISCARD bool isAnimating() const;
    NODISCARD bool isTransitioning() const;

    static QImage generateNoiseTexture(int size);

private:
    void updateTargets();
    void updateFromGame();
    void initMeshes();
    void invalidateCamera();
    void invalidateWeather();

    NODISCARD GLRenderState::Uniforms::Weather::Camera getCameraData(
        const glm::mat4 &viewProj, const Coordinate &playerPos) const;
    void populateWeatherParams(GLRenderState::Uniforms::Weather::Params &params) const;
};
