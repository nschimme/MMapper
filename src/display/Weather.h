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

class NODISCARD WeatherRenderer final
{
private:
    OpenGL &m_gl;
    MapData &m_data;
    const MapCanvasTextures &m_textures;
    GameObserver &m_observer;
    ChangeMonitor::Lifetime m_lifetime;
    QMetaObject::Connection m_posConn;
    QMetaObject::Connection m_forcedPosConn;

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

    float m_animationTime = 0.0f;
    float m_lastUboUploadTime = -1.0f;

    std::optional<GLRenderState::Uniforms::Weather::Static> m_staticUboData;
    glm::mat4 m_lastViewProj{0.0f};

    // Meshes
    std::unique_ptr<Legacy::WeatherSimulationMesh> m_simulation;
    std::unique_ptr<Legacy::WeatherParticleMesh> m_particles;
    UniqueMesh m_atmosphere;
    UniqueMesh m_timeOfDay;

public:
    Signal2<> sig_requestUpdate;

public:
    explicit WeatherRenderer(OpenGL &gl,
                             MapData &data,
                             const MapCanvasTextures &textures,
                             GameObserver &observer,
                             AnimationManager &animationManager);
    ~WeatherRenderer();

    DELETE_CTORS_AND_ASSIGN_OPS(WeatherRenderer);

public:
    void prepare(const glm::mat4 &viewProj);
    void update(float dt);
    NODISCARD bool isAnimating() const;

    void render(const GLRenderState &rs);

public:
    static QImage generateNoiseTexture(int size);

private:
    void initMeshes();
    void invalidateStatic();
};
