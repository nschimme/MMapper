#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "../global/Color.h"
#include "FontFormatFlags.h"

#include <optional>
#include <string>

#include <glm/glm.hpp>

struct NODISCARD GLText final
{
    glm::vec3 pos{0.f};
    std::string text;
    Color color;
    std::optional<Color> bgcolor;
    FontFormatFlags fontFormatFlag;
    int rotationAngle = 0;

    explicit GLText(const glm::vec3 &pos_,
                    std::string moved_text,
                    const Color color_ = {},
                    std::optional<Color> bgcolor_ = {},
                    const FontFormatFlags fontFormatFlag_ = {},
                    int rotationAngle_ = 0)
        : pos{pos_}
        , text{std::move(moved_text)}
        , color{color_}
        , bgcolor{bgcolor_}
        , fontFormatFlag{fontFormatFlag_}
        , rotationAngle{rotationAngle_}
    {}
};
