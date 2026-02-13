// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2024 The MMapper Authors

#include "WeatherRenderer.h"
#include "mapcanvas.h"
#include "../configuration/configuration.h"
#include "../opengl/legacy/Legacy.h"
#include "../opengl/legacy/Shaders.h"
#include <glm/gtc/type_ptr.hpp>

#include <QColor>

WeatherRenderer::WeatherRenderer(MapCanvas &mapCanvas, QObject *parent)
    : QObject(parent)
    , m_mapCanvas(mapCanvas)
{
    connect(&m_animationTimer, &QTimer::timeout, &m_mapCanvas, &MapCanvas::update);

    m_colorAnimation.setDuration(1000);
    connect(&m_colorAnimation, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
        m_overlayColor = value.value<QColor>();
        m_mapCanvas.update();
    });
}

WeatherRenderer::~WeatherRenderer() = default;

#include "../display/Textures.h"

void WeatherRenderer::init(Legacy::Functions &gl) {
    m_fogTexture = m_mapCanvas->allocateTextureId();
    m_snowTexture = m_mapCanvas->allocateTextureId();
    m_cloudTexture = m_mapCanvas->allocateTextureId();
    initParticles(gl);
}

void WeatherRenderer::initParticles(Legacy::Functions &gl) {
    int particleCount = 0;
    switch (getConfig().canvas.effectIntensity.get()) {
        case EffectIntensityEnum::Low:
            particleCount = 500;
            break;
        case EffectIntensityEnum::Medium:
            particleCount = 1000;
            break;
        case EffectIntensityEnum::High:
            particleCount = 2000;
            break;
        default:
            break;
    }
    m_particles.resize(particleCount);
    for (auto &p : m_particles) {
        p.lifetime = 0.0f;
    }

    gl.glGenBuffers(2, m_particleVBOs);
    gl.glGenVertexArrays(2, m_particleVAOs);

    for (int i = 0; i < 2; ++i) {
        gl.glBindVertexArray(m_particleVAOs[i]);
        gl.glBindBuffer(GL_ARRAY_BUFFER, m_particleVBOs[i]);
        gl.glBufferData(GL_ARRAY_BUFFER, m_particles.size() * sizeof(Particle), m_particles.data(), GL_STREAM_DRAW);

        gl.glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Particle), (void*)offsetof(Particle, position));
        gl.glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Particle), (void*)offsetof(Particle, velocity));
        gl.glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, sizeof(Particle), (void*)offsetof(Particle, lifetime));
    }
}

void WeatherRenderer::render(Legacy::Functions &gl) {
    if (getConfig().canvas.effectIntensity.get() == EffectIntensityEnum::Off) {
        m_animationTimer.stop();
        return;
    }

    if (m_overlayColor.alpha() > 0) {
        QColor color = m_overlayColor;
        float opacity = 1.0f;
        switch (getConfig().canvas.effectIntensity.get()) {
            case EffectIntensityEnum::Low:
                opacity = 0.5f;
                break;
            case EffectIntensityEnum::Medium:
                opacity = 0.75f;
                break;
            case EffectIntensityEnum::High:
                opacity = 1.0f;
                break;
            default:
                break;
        }
        color.setAlphaF(color.alphaF() * opacity);
        gl.renderPlainFullScreenQuad(GLRenderState().withBlend(BlendModeEnum::TRANSPARENCY).withColor(Color{color}));
    }

    if (m_weather == WeatherEnum::NICE && m_fog == FogEnum::NO_FOG) {
        if(m_colorAnimation.state() != QAbstractAnimation::Running)
            m_animationTimer.stop();
        return;
    }

    if (!m_animationTimer.isActive()) {
        m_animationTimer.start(16);
    }

    int particle_type = 0;
    float intensity = 1.0f;
    if (m_fog != FogEnum::NO_FOG) {
        particle_type = 1;
        if (m_fog == FogEnum::HEAVY_FOG) {
            intensity = 2.0f;
        }
    } else if (m_weather == WeatherEnum::SNOW) {
        particle_type = 2;
    } else if (m_weather == WeatherEnum::RAIN || m_weather == WeatherEnum::HEAVY_RAIN) {
        if (m_weather == WeatherEnum::HEAVY_RAIN) {
            intensity = 2.0f;
        }
    } else if (m_weather == WeatherEnum::CLOUDS) {
        particle_type = 3;
    }

    auto &particleUpdateShader = gl.getShaderPrograms().getParticleUpdateShader();
    particleUpdateShader->bind();

    // TODO: get weather bubble from somewhere
    glm::vec4 world_bounds = glm::vec4(-10, -10, 20, 20);
    glm::mat4 viewProj = m_mapCanvas.getViewProj();
    glm::vec2 min_screen = glm::vec2(viewProj * glm::vec4(world_bounds.x, world_bounds.y, 0, 1));
    glm::vec2 max_screen = glm::vec2(viewProj * glm::vec4(world_bounds.x + world_bounds.z, world_bounds.y + world_bounds.w, 0, 1));
    m_weatherBounds = glm::vec4(min_screen.x, min_screen.y, max_screen.x - min_screen.x, max_screen.y - min_screen.y);

    particleUpdateShader->setUniform("dt", 0.016f);
    particleUpdateShader->setUniform("wind", glm::vec2(0.1f, 0.0f));
    particleUpdateShader->setUniform("bounding_box", m_weatherBounds);
    particleUpdateShader->setUniform("random_seed", glm::vec2((float)rand(), (float)rand()));
    particleUpdateShader->setUniform("particle_type", particle_type);

    gl.glEnable(GL_RASTERIZER_DISCARD);

    gl.glBindVertexArray(m_particleVAOs[m_currentVBO]);
    gl.glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, m_particleVBOs[1 - m_currentVBO]);

    gl.glBeginTransformFeedback(GL_POINTS);
    int particleCount = m_particles.size() * intensity;
    switch (getConfig().canvas.effectIntensity.get()) {
        case EffectIntensityEnum::Low:
            particleCount /= 4;
            break;
        case EffectIntensityEnum::Medium:
            particleCount /= 2;
            break;
        case EffectIntensityEnum::High:
            break;
        default:
            break;
    }
    gl.glDrawArrays(GL_POINTS, 0, particleCount);
    gl.glEndTransformFeedback();

    gl.glDisable(GL_RASTERIZER_DISCARD);

    auto &particleShader = gl.getShaderPrograms().getParticleShader();
    particleShader->bind();

    particleShader->setUniform("particle_type", particle_type);
    if (particle_type == 1) {
        gl.glBindTexture(GL_TEXTURE_2D, m_fogTexture);
    } else if (particle_type == 2) {
        gl.glBindTexture(GL_TEXTURE_2D, m_snowTexture);
    } else if (particle_type == 3) {
        particleShader->setUniform("time", (float)m_animationTimer.interval());
        gl.renderPlainFullScreenQuad(GLRenderState().withBlend(BlendModeEnum::TRANSPARENCY).withTexture(m_cloudTexture));
    }

    gl.glBindVertexArray(m_particleVAOs[1 - m_currentVBO]);
    gl.glDrawArrays(GL_POINTS, 0, particleCount);

    m_currentVBO = 1 - m_currentVBO;
}

void WeatherRenderer::setWeather(const WeatherEnum weather, const FogEnum fog) {
    m_weather = weather;
    m_fog = fog;
    m_mapCanvas.update();
}

void WeatherRenderer::setTimeOfDay(const MumeTimeEnum time) {
    m_time = time;
    QColor targetColor;
    switch (time) {
        case MumeTimeEnum::DAWN:
            targetColor = QColor(255, 127, 80, 50);
            break;
        case MumeTimeEnum::DAY:
            targetColor = QColor(0, 0, 0, 0);
            break;
        case MumeTimeEnum::DUSK:
            targetColor = QColor(255, 140, 0, 50);
            break;
        case MumeTimeEnum::NIGHT:
            targetColor = QColor(0, 0, 128, 50);
            break;
        default:
            break;
    }

    m_colorAnimation.setStartValue(m_overlayColor);
    m_colorAnimation.setEndValue(targetColor);
    m_colorAnimation.start();
    m_animationTimer.start(16);
}
