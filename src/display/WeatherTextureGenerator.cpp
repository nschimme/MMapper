// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "WeatherTextureGenerator.h"

#include <algorithm>
#include <cmath>

#include <QColor>

namespace WeatherTextureGenerator {

inline float fract(float x)
{
    return x - std::floor(x);
}

inline float lerp(float a, float b, float t)
{
    return a + t * (b - a);
}

inline float hash(float x, float y)
{
    // Exact match for GLSL: fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123)
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

QImage generateNoiseTexture(int size)
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

} // namespace WeatherTextureGenerator
