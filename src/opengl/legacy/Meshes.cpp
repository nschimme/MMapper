// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "Meshes.h"
#include "Legacy.h" // For SharedFunctions, Functions (m_functions is Functions&)
#include "Shaders.h" // For InstancedArrayIconProgram, projection matrix access
#include "../../display/Textures.h" // For SharedMMTexture, MMTextureId, MMTexture (used in virt_render)
#include "../../global/RAII.h" // For RAIICallback
#include "./Binders.h"  // For BlendBinder, DepthBinder
// OpenGLTypes.h is included via Meshes.h or Legacy.h

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

// Constructor Definition
InstancedIconArrayMesh::InstancedIconArrayMesh(SharedFunctions funcs, std::shared_ptr<InstancedArrayIconProgram> prog, MMTextureId array_tex_id)
    : m_shared_functions(std::move(funcs)),
      m_functions(deref(m_shared_functions)),
      m_program(std::move(prog)),
      m_array_texture_id(array_tex_id),
      m_vao(0),
      m_base_quad_vbo(0),
      m_base_quad_ibo(0),
      m_instance_data_vbo(0),
      m_instance_count(0),
      m_base_quad_index_count(0)
{
    initMesh(); // Call to private helper method (defined inline in header)
}

// Destructor Definition
InstancedIconArrayMesh::~InstancedIconArrayMesh() {
    cleanupMesh(); // Call to private helper method (defined inline in header)
}

// virt_clear Definition
void InstancedIconArrayMesh::virt_clear() {
    updateInstances({}); // updateInstances is inline in header
}

// virt_reset Definition
void InstancedIconArrayMesh::virt_reset() {
    cleanupMesh(); // Call to private helper method (defined inline in header)
    m_instance_count = 0;
    // m_base_quad_index_count is set by initMesh based on static data,
    // so it's implicitly reset if initMesh were called again.
    // For this reset, just clearing instance count and GL resources is enough.
}

// virt_isEmpty Definition
bool InstancedIconArrayMesh::virt_isEmpty() const {
    return m_instance_count == 0;
}

// virt_render Definition
void InstancedIconArrayMesh::virt_render(const GLRenderState& state) {
    if (virt_isEmpty()) { // uses the virtual method
        return;
    }

    m_functions.checkError();
    RAIICallback programUnbinder = m_program->bind(); // m_program is std::shared_ptr<InstancedArrayIconProgram>

    m_program->setProjectionViewMatrix(m_functions.getProjectionMatrix());

    float icon_size = state.uniforms.pointSize.value_or(32.0f); // Default to 32 if not specified
    m_program->setIconBaseSize(icon_size);

    auto& texLookup = m_functions.getTexLookup();

    if (m_array_texture_id != INVALID_MM_TEXTURE_ID) {
        // Ensure index is valid before attempting to access texLookup
        // (Assuming MMTextureId.value() gives a usable index type)
        if (m_array_texture_id.value() >= 0 && static_cast<size_t>(m_array_texture_id.value()) < texLookup.size()) {
            const SharedMMTexture& sharedTex = texLookup.at(m_array_texture_id);
            if (sharedTex) { // Check if sharedTex is not null
                // The MMTexture::bind() method has a TODO for adopted textures (it's currently a no-op).
                // If this texture is adopted, this bind call will not perform an actual OpenGL bind.
                // This needs to be handled by rendering logic specific to adopted textures if direct GL calls are needed.
                sharedTex->bind(0); // Bind to texture unit 0
            }
        }
        m_program->setTextureSampler(0); // Tell shader to use texture unit 0
    }

    BlendBinder blendBinder(m_functions, state.blend);
    DepthBinder depthBinder(m_functions, state.depth);

    m_functions.glBindVertexArray(m_vao);
    RAIICallback vaoUnbinder([&]() {
        m_functions.glBindVertexArray(0);
        if (m_array_texture_id != INVALID_MM_TEXTURE_ID) {
             auto& currentTexLookup = m_functions.getTexLookup();
             if (m_array_texture_id.value() >= 0 && static_cast<size_t>(m_array_texture_id.value()) < currentTexLookup.size()) {
                const SharedMMTexture& sharedTex = currentTexLookup.at(m_array_texture_id);
                if (sharedTex) { // Check if sharedTex is not null
                   sharedTex->release(0); // Release from texture unit 0
                }
             }
        }
        m_functions.checkError();
    });

    m_functions.glDrawElementsInstanced(GL_TRIANGLES, m_base_quad_index_count, GL_UNSIGNED_INT, nullptr, m_instance_count);
}

} // namespace Legacy
