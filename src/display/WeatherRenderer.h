#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2024 The MMapper Authors

#include "../weather/WeatherState.h"
#include "../clock/mumemoment.h"
#include "../global/macros.h"
#include "../opengl/OpenGLTypes.h"
#include <glm/glm.hpp>

#include <QObject>
#include <QTimer>
#include <QVariantAnimation>
#include <vector>

namespace Legacy {
class Functions;
}
class MapCanvas;

class NODISCARD_QOBJECT WeatherRenderer final : public QObject {
    Q_OBJECT
private:
    MapCanvas &m_mapCanvas;
    WeatherEnum m_weather = WeatherEnum::NICE;
    FogEnum m_fog = FogEnum::NO_FOG;
    MumeTimeEnum m_time = MumeTimeEnum::UNKNOWN;
    QTimer m_animationTimer;
    QVariantAnimation m_colorAnimation;
    QColor m_overlayColor;

    struct Particle {
        glm::vec3 position;
        glm::vec3 velocity;
        float lifetime;
    };

    std::vector<Particle> m_particles;
    GLuint m_particleVBOs[2];
    GLuint m_particleVAOs[2];
    int m_currentVBO = 0;
    MMTextureId m_fogTexture = INVALID_MM_TEXTURE_ID;
    MMTextureId m_snowTexture = INVALID_MM_TEXTURE_ID;
    MMTextureId m_cloudTexture = INVALID_MM_TEXTURE_ID;
    glm::vec4 m_weatherBounds;

    void initParticles(Legacy::Functions &gl);
public:
    explicit WeatherRenderer(MapCanvas &mapCanvas, QObject *parent);
    ~WeatherRenderer() final;

    void render(Legacy::Functions &gl);
    void init(Legacy::Functions &gl);

public slots:
    void setWeather(WeatherEnum weather, FogEnum fog);
    void setTimeOfDay(MumeTimeEnum time);
};