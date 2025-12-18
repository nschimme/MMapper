#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2023 The MMapper Authors

#include <glm/glm.hpp>

namespace modern
{

struct RoomInstanceData
{
    glm::vec3 position;
    glm::vec3 tex_coord;
    glm::vec4 color;
};

} // namespace modern
