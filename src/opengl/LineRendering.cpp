// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#include "LineRendering.h"

#include <cassert>

#include <glm/glm.hpp>
#include <glm/gtx/norm.hpp>

namespace mmgl {

void generateLine(std::vector<LineVert> &verts,
                  const glm::vec3 &p1,
                  const glm::vec3 &p2,
                  float width,
                  const Color &color)
{
    verts.emplace_back(p1, p2, color, width, glm::vec2(-1, 0));
    verts.emplace_back(p1, p2, color, width, glm::vec2(1, 0));
    verts.emplace_back(p1, p2, color, width, glm::vec2(1, 1));
    verts.emplace_back(p1, p2, color, width, glm::vec2(-1, 1));
}

} // namespace mmgl
