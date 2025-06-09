// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "Meshes.h"
#include "../OpenGLTypes.h" // For BaseQuadVert, IconInstanceData

namespace Legacy {

// Static member definitions for InstancedIconArrayMesh
const std::vector<BaseQuadVert> InstancedIconArrayMesh::s_base_quad_verts = {
    {{0.0f, 0.0f}, {0.0f, 0.0f}}, // pos, uv : bottom-left
    {{1.0f, 0.0f}, {1.0f, 0.0f}}, // pos, uv : bottom-right
    {{1.0f, 1.0f}, {1.0f, 1.0f}}, // pos, uv : top-right
    {{0.0f, 1.0f}, {0.0f, 1.0f}}  // pos, uv : top-left
};

const std::vector<unsigned int> InstancedIconArrayMesh::s_base_quad_indices = {
    0, 1, 2, // First triangle
    2, 3, 0  // Second triangle
};

} // namespace Legacy
