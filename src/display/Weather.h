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
#include "../opengl/legacy/WeatherMeshes.h"
#include "AnimationManager.h"
#include "Textures.h"

#include <chrono>
#include <memory>

#include <QMetaObject>

class MapData;

/**
 * @brief Manages the logic and state of the weather system.
 * Tracks game weather signals, handles transitions, and provides data for UBOs.
 */
class NODISCARD WeatherSystem final
{
private:
    GameObserver &m_observer;
    AnimationManager &m_animationManager;
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

public:
    Signal2<> sig_stateInvalidated;

public:
    explicit WeatherSystem(GameObserver &observer, AnimationManager &animationManager);
    ~WeatherSystem();
    DELETE_CTORS_AND_ASSIGN_OPS(WeatherSystem);

public:
    void update();
    NODISCARD bool isAnimating() const;

    NODISCARD GLRenderState::Uniforms::Weather::Static getStaticUboData(
        const glm::mat4 &viewProj, const Coordinate &playerPos) const;

    // Getters for rendering
    NODISCARD float getCurrentRainIntensity() const { return m_currentRainIntensity; }
    NODISCARD float getCurrentSnowIntensity() const { return m_currentSnowIntensity; }
    NODISCARD float getCurrentCloudsIntensity() const { return m_currentCloudsIntensity; }
    NODISCARD float getCurrentFogIntensity() const { return m_currentFogIntensity; }
    NODISCARD float getCurrentTimeOfDayIntensity() const { return m_currentTimeOfDayIntensity; }
    NODISCARD MumeTimeEnum getCurrentTimeOfDay() const { return m_currentTimeOfDay; }
    NODISCARD MumeTimeEnum getOldTimeOfDay() const { return m_oldTimeOfDay; }

    NODISCARD const Signal2Lifetime &getSignalLifetime() const { return m_signalLifetime; }

private:
    void updateTargets();
    void updateFromGame();
};

/**
 * @brief Facade that coordinates WeatherSystem, WeatherMeshes, and UboManager.
 */
class NODISCARD WeatherRenderer final
{
private:
    OpenGL &m_gl;
    MapData &m_data;
    const MapCanvasTextures &m_textures;
    std::unique_ptr<WeatherSystem> m_system;
    AnimationManager &m_animationManager;

    QMetaObject::Connection m_posConn;
    QMetaObject::Connection m_forcedPosConn;

    glm::mat4 m_lastViewProj{0.0f};

    // Meshes
    std::unique_ptr<Legacy::ParticleSimulationMesh> m_simulation;
    std::unique_ptr<Legacy::ParticleRenderMesh> m_particles;
    UniqueMesh m_atmosphere;
    UniqueMesh m_timeOfDay;

public:
    Signal2<> sig_requestUpdate;

public:
    explicit WeatherRenderer(OpenGL &gl,
                             MapData &mapData,
                             const MapCanvasTextures &textures,
                             GameObserver &observer,
                             AnimationManager &animationManager);
    ~WeatherRenderer();

    DELETE_CTORS_AND_ASSIGN_OPS(WeatherRenderer);

public:
    void prepare(const glm::mat4 &viewProj);
    void update(float frameDeltaTime);
    void render(const GLRenderState &rs);

public:
    static QImage generateNoiseTexture(int size);

private:
    void initMeshes();
    void invalidateStatic();
};
