#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "../global/Color.h"
#include "FontFormatFlags.h"

#include <optional>
#include <string>

#include <glm/glm.hpp>

// Domain-specific data for rendering a string.
struct NODISCARD GLText final
{
    // Position of the text.
    // NOTE: This can represent either world space or screen space coordinates
    // depending on the rendering context (e.g., world-space batching vs. immediate 2D).
    glm::vec3 pos{};
    std::string text;
    Color color;
    std::optional<Color> bgcolor;
    FontFormatFlags fontFormatFlag;
    int rotationAngle = 0; // degrees

    explicit GLText(const glm::vec3 &pos_,
                    std::string text_,
                    const Color color_,
                    std::optional<Color> bgcolor_ = std::nullopt,
                    const FontFormatFlags fontFormatFlag_ = {},
                    const int rotationAngle_ = 0)
        : pos{pos_}
        , text{std::move(text_)}
        , color{color_}
        , bgcolor{bgcolor_}
        , fontFormatFlag{fontFormatFlag_}
        , rotationAngle{rotationAngle_}
    {}
};
