#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2023 The MMapper Authors

#include "../observer/gameobserver.h"
#include <memory>

namespace Legacy {
class Functions;
}

class MapCanvas;

#include "../global/Signal2.h"
#include "../opengl/legacy/VAO.h"
#include "../opengl/legacy/VBO.h"

class GameObserver;

namespace Legacy {
class VAO;
class VBO;
}

class NODISCARD WeatherRenderer final {
public:
    explicit WeatherRenderer(MapCanvas &canvas, Legacy::Functions &gl, GameObserver &observer);
    ~WeatherRenderer();

    void initialize();
    void render();

private:
    void onTimeOfDayChanged(MumeTimeEnum timeOfDay);
    void onWeatherChanged(PromptWeatherEnum weather);
    void onFogChanged(PromptFogEnum fog);

private:
    class WeatherRendererImpl;
    std::unique_ptr<WeatherRendererImpl> m_impl;
    Signal2Lifetime m_lifetime;
};
