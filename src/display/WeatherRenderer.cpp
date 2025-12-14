// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2023 The MMapper Authors

#include "WeatherRenderer.h"
#include "../opengl/legacy/Legacy.h"
#include "mapcanvas.h"

#include "../observer/gameobserver.h"

#include "../opengl/legacy/Shaders.h"
#include "../configuration/configuration.h"

class WeatherRenderer::WeatherRendererImpl {
#include <random>

public:
    WeatherRendererImpl(MapCanvas &canvas, Legacy::Functions &gl)
        : m_canvas(canvas), m_gl(gl) {}

    void initialize() {
        loadShaders();
        m_vao = std::make_unique<Legacy::VAO>();
        m_vao->emplace(m_gl.shared_from_this());

        // Create particles
        std::vector<float> particleData;
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<float> dis(-100.0f, 100.0f);
        std::uniform_real_distribution<float> dis_y(0.0f, 200.0f);

        for (int i = 0; i < 1000; ++i) {
            particleData.push_back(dis(gen)); // start x
            particleData.push_back(dis_y(gen)); // start y
            particleData.push_back(dis(gen)); // start z
            particleData.push_back(0.0f); // velocity x
            particleData.push_back(-10.0f); // velocity y
            particleData.push_back(0.0f); // velocity z
        }

        m_particleVbo = std::make_unique<Legacy::VBO>();
        m_particleVbo->emplace(m_gl.shared_from_this());
        m_gl.glBindBuffer(GL_ARRAY_BUFFER, m_particleVbo->get());
        m_gl.glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(particleData.size() * sizeof(float)), particleData.data(), GL_STATIC_DRAW);
    }

    void render() {
        const auto &config = getConfig().canvas.advanced.weatherEffects;
        if (config.showTimeOfDay.get()) {
            Color color;
            switch (m_timeOfDay) {
            case MumeTimeEnum::DAWN:
                color = Color(0.8f, 0.8f, 0.4f, 0.2f);
                break;
            case MumeTimeEnum::DUSK:
                color = Color(0.8f, 0.4f, 0.4f, 0.2f);
                break;
            case MumeTimeEnum::NIGHT:
                color = Color(0.2f, 0.2f, 0.8f, 0.2f);
                break;
            case MumeTimeEnum::UNKNOWN:
            case MumeTimeEnum::DAY:
                break;
            }
            if (color.getAlpha() > 0) {
                auto binder = m_overlayShader->bind();
                m_overlayShader->setColor("uColor", color);
                m_gl.glBindVertexArray(m_vao->get());
                m_gl.glDrawArrays(GL_TRIANGLES, 0, 6);
            }
        }

        // Render particles
        bool rain = config.showRain.get() && (m_weather == PromptWeatherEnum::RAIN || m_weather == PromptWeatherEnum::HEAVY_RAIN);
        bool snow = config.showSnow.get() && m_weather == PromptWeatherEnum::SNOW;

        if (!rain && !snow) {
            return;
        }

        auto particleBinder = m_particleShader->bind();
        m_elapsedTime += m_canvas.getElapsedTime();
        GLint timeLoc = m_particleShader->getUniformLocation("uTime");
        m_particleShader->setUniform1fv(timeLoc, 1, &m_elapsedTime);

        m_gl.glBindVertexArray(m_vao->get());
        m_gl.glBindBuffer(GL_ARRAY_BUFFER, m_particleVbo->get());

        GLuint posAttrib = m_particleShader->getAttribLocation("aStartPosition");
        m_gl.glEnableVertexAttribArray(posAttrib);
        m_gl.glVertexAttribPointer(posAttrib, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), nullptr);

        GLuint velAttrib = m_particleShader->getAttribLocation("aVelocity");
        m_gl.glEnableVertexAttribArray(velAttrib);
        m_gl.glVertexAttribPointer(velAttrib, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), reinterpret_cast<void*>(3 * sizeof(float)));

        if (rain) {
            m_gl.glDrawArrays(GL_POINTS, 0, 500);
        }
        if (snow) {
            m_gl.glDrawArrays(GL_POINTS, 500, 500);
        }

        if (config.showFog.get() && (m_fog == PromptFogEnum::LIGHT_FOG || m_fog == PromptFogEnum::HEAVY_FOG)) {
            auto fogBinder = m_fogShader->bind();
            GLint fogTimeLoc = m_fogShader->getUniformLocation("uTime");
            m_fogShader->setUniform1fv(fogTimeLoc, 1, &m_elapsedTime);
            m_gl.glBindVertexArray(m_vao->get());
            m_gl.glDrawArrays(GL_TRIANGLES, 0, 6);
        }
    }

    void onTimeOfDayChanged(MumeTimeEnum timeOfDay) {
        m_timeOfDay = timeOfDay;
        updateAnimation();
    }

    void onWeatherChanged(PromptWeatherEnum weather) {
        m_weather = weather;
        updateAnimation();
    }

    void onFogChanged(PromptFogEnum fog) {
        m_fog = fog;
        updateAnimation();
    }

private:
    void loadShaders() {
        auto &shaderPrograms = m_gl.getShaderPrograms();
        m_overlayShader = shaderPrograms.getShader("weather/overlay");
        m_particleShader = shaderPrograms.getShader("weather/particle");
        m_fogShader = shaderPrograms.getShader("weather/fog");
    }

    void updateAnimation() {
        const auto &config = getConfig().canvas.advanced.weatherEffects;
        bool animating = false;
        if (config.showTimeOfDay.get() && m_timeOfDay != MumeTimeEnum::DAY) {
            animating = true;
        }
        if (config.showRain.get() && (m_weather == PromptWeatherEnum::RAIN || m_weather == PromptWeatherEnum::HEAVY_RAIN)) {
            animating = true;
        }
        if (config.showSnow.get() && m_weather == PromptWeatherEnum::SNOW) {
            animating = true;
        }
        if (config.showFog.get() && (m_fog == PromptFogEnum::LIGHT_FOG || m_fog == PromptFogEnum::HEAVY_FOG)) {
            animating = true;
        }
        m_canvas.setAnimating(animating);
    }

private:
    MapCanvas &m_canvas;
    Legacy::Functions &m_gl;

    std::shared_ptr<Legacy::AbstractShaderProgram> m_overlayShader;
    std::shared_ptr<Legacy::AbstractShaderProgram> m_particleShader;
    std::shared_ptr<Legacy::AbstractShaderProgram> m_fogShader;

    std::unique_ptr<Legacy::VAO> m_vao;
    std::unique_ptr<Legacy::VBO> m_particleVbo;

    MumeTimeEnum m_timeOfDay = MumeTimeEnum::UNKNOWN;
    PromptWeatherEnum m_weather = PromptWeatherEnum::NICE;
    PromptFogEnum m_fog = PromptFogEnum::NO_FOG;
    float m_elapsedTime = 0.0f;
};

WeatherRenderer::WeatherRenderer(MapCanvas &canvas, Legacy::Functions &gl, GameObserver &observer)
    : m_impl(std::make_unique<WeatherRendererImpl>(canvas, gl))
{
    observer.sig2_timeOfDayChanged.connect(m_lifetime, [this](MumeTimeEnum timeOfDay) {
        onTimeOfDayChanged(timeOfDay);
    });
    observer.sig2_weatherChanged.connect(m_lifetime, [this](PromptWeatherEnum weather) {
        onWeatherChanged(weather);
    });
    observer.sig2_fogChanged.connect(m_lifetime, [this](PromptFogEnum fog) {
        onFogChanged(fog);
    });
}

WeatherRenderer::~WeatherRenderer() = default;

void WeatherRenderer::initialize() {
    m_impl->initialize();
}

void WeatherRenderer::render() {
    m_impl->render();
}

void WeatherRenderer::onTimeOfDayChanged(MumeTimeEnum timeOfDay) {
    m_impl->onTimeOfDayChanged(timeOfDay);
}

void WeatherRenderer::onWeatherChanged(PromptWeatherEnum weather) {
    m_impl->onWeatherChanged(weather);
}

void WeatherRenderer::onFogChanged(PromptFogEnum fog) {
    m_impl->onFogChanged(fog);
}
