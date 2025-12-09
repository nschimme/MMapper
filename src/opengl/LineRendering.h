// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#pragma once

#include "../opengl/OpenGLTypes.h"

#include <vector>

#include <glm/glm.hpp>

namespace mmgl {

void generateLine(std::vector<LineVert> &verts,
                  const glm::vec3 &p1,
                  const glm::vec3 &p2,
                  float width,
                  const Color &color);

} // namespace mmgl
