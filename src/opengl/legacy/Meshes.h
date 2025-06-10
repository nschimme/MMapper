#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

// Enforced include order for Meshes.h
#include "../OpenGLTypes.h"    // 1. Basic GL types, enums, IRenderable, forward declarations (MMTexture), BaseQuadVert etc.
#include "../../display/Textures.h" // 2. Full MMTexture definition
#include "./Binders.h"          // 3. BlendBinder, DepthBinder
#include "./Shaders.h"          // 4. Shader program classes (InstancedArrayIconProgram, AbstractShaderProgram)
// Note: Legacy.h is included by Shaders.h or other files using Legacy::Functions.
// SimpleMesh.h is included after this block for other mesh definitions in this file.

// Standard library
#include <vector>
#include <memory>
#include <optional>
#include <string> // May be needed by some mesh logic, good to have.
#include <stdexcept> // For runtime_error

// Global utilities
#include "../../global/RAII.h"    // For RAIICallback
#include "../../global/utils.h"   // For deref, etc.

// Must come after primary types are defined, especially IRenderable from OpenGLTypes.h
#include "./SimpleMesh.h"       // For base classes of other mesh types defined in this file (PlainMesh, etc.)

#define VOIDPTR_OFFSETOF(x, y) reinterpret_cast<void *>(offsetof(x, y))
#define VPO(x) VOIDPTR_OFFSETOF(VertexType_, x)

namespace Legacy {

// Uniform color
template<typename VertexType_>
class NODISCARD PlainMesh final : public SimpleMesh<VertexType_, UColorPlainShader>
{
public:
    using Base = SimpleMesh<VertexType_, UColorPlainShader>;
    using Base::Base;

private:
    struct NODISCARD Attribs final
    {
        GLuint vertPos = INVALID_ATTRIB_LOCATION;

        NODISCARD static Attribs getLocations(AbstractShaderProgram &fontShader)
        {
            Attribs result;
            result.vertPos = fontShader.getAttribLocation("aVert");
            return result;
        }
    };

    std::optional<Attribs> boundAttribs;

    void virt_bind() override
    {
        static_assert(sizeof(std::declval<VertexType_>()) == 3 * sizeof(GLfloat));

        Functions &gl = Base::m_functions;
        const auto attribs = Attribs::getLocations(Base::m_program);
        gl.glBindBuffer(GL_ARRAY_BUFFER, Base::m_vbo.get());
        gl.enableAttrib(attribs.vertPos, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
        boundAttribs = attribs;
    }

    void virt_unbind() override
    {
        if (!boundAttribs) {
            assert(false);
            return;
        }

        auto &attribs = boundAttribs.value();
        Functions &gl = Base::m_functions;
        gl.glDisableVertexAttribArray(attribs.vertPos);
        gl.glBindBuffer(GL_ARRAY_BUFFER, 0);
        boundAttribs.reset();
    }
};

// Per-vertex color
// flat-shaded in MMapper, due to glShadeModel(GL_FLAT)
template<typename VertexType_>
class NODISCARD ColoredMesh final : public SimpleMesh<VertexType_, AColorPlainShader>
{
public:
    using Base = SimpleMesh<VertexType_, AColorPlainShader>;
    using Base::Base;

private:
    struct NODISCARD Attribs final
    {
        GLuint colorPos = INVALID_ATTRIB_LOCATION;
        GLuint vertPos = INVALID_ATTRIB_LOCATION;

        NODISCARD static Attribs getLocations(AbstractShaderProgram &fontShader)
        {
            Attribs result;
            result.colorPos = fontShader.getAttribLocation("aColor");
            result.vertPos = fontShader.getAttribLocation("aVert");
            return result;
        }
    };

    std::optional<Attribs> boundAttribs;

    void virt_bind() override
    {
        const auto vertSize = static_cast<GLsizei>(sizeof(VertexType_));
        static_assert(sizeof(std::declval<VertexType_>().color) == 4 * sizeof(uint8_t));
        static_assert(sizeof(std::declval<VertexType_>().vert) == 3 * sizeof(GLfloat));

        Functions &gl = Base::m_functions;
        const auto attribs = Attribs::getLocations(Base::m_program);
        gl.glBindBuffer(GL_ARRAY_BUFFER, Base::m_vbo.get());
        gl.enableAttrib(attribs.colorPos, 4, GL_UNSIGNED_BYTE, GL_TRUE, vertSize, VPO(color));
        gl.enableAttrib(attribs.vertPos, 3, GL_FLOAT, GL_FALSE, vertSize, VPO(vert));
        boundAttribs = attribs;
    }

    void virt_unbind() override
    {
        if (!boundAttribs) {
            assert(false);
            return;
        }

        auto &attribs = boundAttribs.value();
        Functions &gl = Base::m_functions;
        gl.glDisableVertexAttribArray(attribs.colorPos);
        gl.glDisableVertexAttribArray(attribs.vertPos);
        gl.glBindBuffer(GL_ARRAY_BUFFER, 0);
        boundAttribs.reset();
    }
}; // namespace Legacy

// Textured mesh with color modulated by uniform
template<typename VertexType_>
class NODISCARD TexturedMesh final : public SimpleMesh<VertexType_, UColorTexturedShader>
{
public:
    using Base = SimpleMesh<VertexType_, UColorTexturedShader>;
    using Base::Base;

private:
    struct NODISCARD Attribs final
    {
        GLuint texPos = INVALID_ATTRIB_LOCATION;
        GLuint vertPos = INVALID_ATTRIB_LOCATION;

        NODISCARD static Attribs getLocations(AbstractShaderProgram &fontShader)
        {
            Attribs result;
            result.texPos = fontShader.getAttribLocation("aTexCoord");
            result.vertPos = fontShader.getAttribLocation("aVert");
            return result;
        }
    };

    std::optional<Attribs> boundAttribs;

    void virt_bind() override
    {
        const auto vertSize = static_cast<GLsizei>(sizeof(VertexType_));
        static_assert(sizeof(std::declval<VertexType_>().tex) == 2 * sizeof(GLfloat));
        static_assert(sizeof(std::declval<VertexType_>().vert) == 3 * sizeof(GLfloat));

        Functions &gl = Base::m_functions;
        const auto attribs = Attribs::getLocations(Base::m_program);
        gl.glBindBuffer(GL_ARRAY_BUFFER, Base::m_vbo.get());
        /* NOTE: OpenGL 2.x can't use GL_TEXTURE_2D_ARRAY. */
        gl.enableAttrib(attribs.texPos, 2, GL_FLOAT, GL_FALSE, vertSize, VPO(tex));
        gl.enableAttrib(attribs.vertPos, 3, GL_FLOAT, GL_FALSE, vertSize, VPO(vert));
        boundAttribs = attribs;
    }

    void virt_unbind() override
    {
        if (!boundAttribs) {
            assert(false);
            return;
        }

        auto &attribs = boundAttribs.value();
        Functions &gl = Base::m_functions;
        gl.glDisableVertexAttribArray(attribs.texPos);
        gl.glDisableVertexAttribArray(attribs.vertPos);
        gl.glBindBuffer(GL_ARRAY_BUFFER, 0);
        boundAttribs.reset();
    }
};

// Textured mesh with color modulated by color attribute.
template<typename VertexType_>
class NODISCARD ColoredTexturedMesh final : public SimpleMesh<VertexType_, AColorTexturedShader>
{
public:
    using Base = SimpleMesh<VertexType_, AColorTexturedShader>;
    using Base::Base;

private:
    struct NODISCARD Attribs final
    {
        GLuint colorPos = INVALID_ATTRIB_LOCATION;
        GLuint texPos = INVALID_ATTRIB_LOCATION;
        GLuint vertPos = INVALID_ATTRIB_LOCATION;

        NODISCARD static Attribs getLocations(AColorTexturedShader &fontShader)
        {
            Attribs result;
            result.colorPos = fontShader.getAttribLocation("aColor");
            result.texPos = fontShader.getAttribLocation("aTexCoord");
            result.vertPos = fontShader.getAttribLocation("aVert");
            return result;
        }
    };

    std::optional<Attribs> boundAttribs;

    void virt_bind() override
    {
        const auto vertSize = static_cast<GLsizei>(sizeof(VertexType_));
        static_assert(sizeof(std::declval<VertexType_>().color) == 4 * sizeof(uint8_t));
        static_assert(sizeof(std::declval<VertexType_>().tex) == 2 * sizeof(GLfloat));
        static_assert(sizeof(std::declval<VertexType_>().vert) == 3 * sizeof(GLfloat));

        Functions &gl = Base::m_functions;
        const auto attribs = Attribs::getLocations(Base::m_program);
        gl.glBindBuffer(GL_ARRAY_BUFFER, Base::m_vbo.get());
        gl.enableAttrib(attribs.colorPos, 4, GL_UNSIGNED_BYTE, GL_TRUE, vertSize, VPO(color));
        /* NOTE: OpenGL 2.x can't use GL_TEXTURE_2D_ARRAY. */
        gl.enableAttrib(attribs.texPos, 2, GL_FLOAT, GL_FALSE, vertSize, VPO(tex));
        gl.enableAttrib(attribs.vertPos, 3, GL_FLOAT, GL_FALSE, vertSize, VPO(vert));
        boundAttribs = attribs;
    }

    void virt_unbind() override
    {
        if (!boundAttribs) {
            assert(false);
            return;
        }

        auto &attribs = boundAttribs.value();
        Functions &gl = Base::m_functions;
        gl.glDisableVertexAttribArray(attribs.colorPos);
        gl.glDisableVertexAttribArray(attribs.texPos);
        gl.glDisableVertexAttribArray(attribs.vertPos);
        gl.glBindBuffer(GL_ARRAY_BUFFER, 0);
        boundAttribs.reset();
    }
};

// Per-vertex color
// flat-shaded in MMapper, due to glShadeModel(GL_FLAT)
template<typename VertexType_>
class NODISCARD PointMesh final : public SimpleMesh<VertexType_, PointShader>
{
public:
    using Base = SimpleMesh<VertexType_, PointShader>;
    using Base::Base;

private:
    struct NODISCARD Attribs final
    {
        GLuint colorPos = INVALID_ATTRIB_LOCATION;
        GLuint vertPos = INVALID_ATTRIB_LOCATION;

        NODISCARD static Attribs getLocations(AbstractShaderProgram &fontShader)
        {
            Attribs result;
            result.colorPos = fontShader.getAttribLocation("aColor");
            result.vertPos = fontShader.getAttribLocation("aVert");
            return result;
        }
    };

    std::optional<Attribs> boundAttribs;

    void virt_bind() override
    {
        const auto vertSize = static_cast<GLsizei>(sizeof(VertexType_));
        static_assert(sizeof(std::declval<VertexType_>().color) == 4 * sizeof(uint8_t));
        static_assert(sizeof(std::declval<VertexType_>().vert) == 3 * sizeof(GLfloat));

        Functions &gl = Base::m_functions;
        const auto attribs = Attribs::getLocations(Base::m_program);
        gl.glBindBuffer(GL_ARRAY_BUFFER, Base::m_vbo.get());
        gl.enableAttrib(attribs.colorPos, 4, GL_UNSIGNED_BYTE, GL_TRUE, vertSize, VPO(color));
        gl.enableAttrib(attribs.vertPos, 3, GL_FLOAT, GL_FALSE, vertSize, VPO(vert));
        boundAttribs = attribs;
    }

    void virt_unbind() override
    {
        if (!boundAttribs) {
            assert(false);
            return;
        }

        auto &attribs = boundAttribs.value();
        Functions &gl = Base::m_functions;
        gl.glDisableVertexAttribArray(attribs.colorPos);
        gl.glDisableVertexAttribArray(attribs.vertPos);
        gl.glBindBuffer(GL_ARRAY_BUFFER, 0);
        boundAttribs.reset();
    }
};

// This replacement will remove the first definition of InstancedIconArrayMesh
// (which was less complete and caused the redefinition error).
// The second, more complete definition (already added in a previous step) will remain.
// The key is that the SEARCH block must exactly match the first definition.
// The first definition (the one to remove) is the one that does NOT have the inline implementations.

class NODISCARD InstancedIconArrayMesh final : public IRenderable {
public:
    InstancedIconArrayMesh(SharedFunctions funcs, std::shared_ptr<InstancedArrayIconProgram> prog, MMTextureId array_tex_id); // Declare only (or keep inline if simple)

    ~InstancedIconArrayMesh() override; // Declare only

    void updateInstances(const std::vector<IconInstanceData>& instance_data) { // Can remain inline
        m_functions.glBindBuffer(GL_ARRAY_BUFFER, m_instance_data_vbo);
        m_functions.glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(instance_data.size() * sizeof(IconInstanceData)), instance_data.data(), GL_STREAM_DRAW);
        m_functions.glBindBuffer(GL_ARRAY_BUFFER, 0);
        m_instance_count = static_cast<GLsizei>(instance_data.size());
    }

    MMTextureId getTextureId() const { return m_array_texture_id; } // Can remain inline

protected:
    // IRenderable overrides
    void virt_clear() override; // Declare only
    void virt_reset() override; // Declare only
    bool virt_isEmpty() const override; // Declare only
    void virt_render(const GLRenderState& state) override; // Declare only

private:
    SharedFunctions m_shared_functions;
    Functions &m_functions;
    std::shared_ptr<InstancedArrayIconProgram> m_program;
    MMTextureId m_array_texture_id;

    GLuint m_vao = 0;
    GLuint m_base_quad_vbo = 0;
    GLuint m_base_quad_ibo = 0;
    GLuint m_instance_data_vbo = 0;

    GLsizei m_instance_count = 0;
    GLsizei m_base_quad_index_count = 0;

    // Static data for the base quad
    static const std::vector<BaseQuadVert> s_base_quad_verts; // Declaration
    static const std::vector<unsigned int> s_base_quad_indices; // Declaration

    void initMesh() { // Private helper, can remain inline
        m_functions.glGenVertexArrays(1, &m_vao);
        m_functions.glBindVertexArray(m_vao);

        m_functions.glGenBuffers(1, &m_base_quad_vbo);
        m_functions.glBindBuffer(GL_ARRAY_BUFFER, m_base_quad_vbo);
        m_functions.glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(s_base_quad_verts.size() * sizeof(BaseQuadVert)), s_base_quad_verts.data(), GL_STATIC_DRAW);

        m_functions.glGenBuffers(1, &m_base_quad_ibo);
        m_functions.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_base_quad_ibo);
        m_functions.glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(s_base_quad_indices.size() * sizeof(unsigned int)), s_base_quad_indices.data(), GL_STATIC_DRAW);
        m_base_quad_index_count = static_cast<GLsizei>(s_base_quad_indices.size());

        m_functions.glEnableVertexAttribArray(AttributesEnum::ATTR_BASE_QUAD_POSITION);
        m_functions.glVertexAttribPointer(AttributesEnum::ATTR_BASE_QUAD_POSITION, 2, GL_FLOAT, GL_FALSE, sizeof(BaseQuadVert), reinterpret_cast<const GLvoid *>(offsetof(BaseQuadVert, position)));

        m_functions.glEnableVertexAttribArray(AttributesEnum::ATTR_BASE_QUAD_UV);
        m_functions.glVertexAttribPointer(AttributesEnum::ATTR_BASE_QUAD_UV, 2, GL_FLOAT, GL_FALSE, sizeof(BaseQuadVert), reinterpret_cast<const GLvoid *>(offsetof(BaseQuadVert, uv)));

        m_functions.glGenBuffers(1, &m_instance_data_vbo);
        m_functions.glBindBuffer(GL_ARRAY_BUFFER, m_instance_data_vbo);
        m_functions.glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_STREAM_DRAW);

        m_functions.glEnableVertexAttribArray(AttributesEnum::ATTR_INSTANCE_WORLD_POS_CENTER);
        m_functions.glVertexAttribPointer(AttributesEnum::ATTR_INSTANCE_WORLD_POS_CENTER, 3, GL_FLOAT, GL_FALSE, sizeof(IconInstanceData), reinterpret_cast<const GLvoid *>(offsetof(IconInstanceData, world_position_center)));
        m_functions.glVertexAttribDivisor(AttributesEnum::ATTR_INSTANCE_WORLD_POS_CENTER, 1);

        m_functions.glEnableVertexAttribArray(AttributesEnum::ATTR_INSTANCE_TEX_LAYER_INDEX);
        m_functions.glVertexAttribPointer(AttributesEnum::ATTR_INSTANCE_TEX_LAYER_INDEX, 1, GL_FLOAT, GL_FALSE, sizeof(IconInstanceData), reinterpret_cast<const GLvoid *>(offsetof(IconInstanceData, tex_layer_index)));
        m_functions.glVertexAttribDivisor(AttributesEnum::ATTR_INSTANCE_TEX_LAYER_INDEX, 1);

        m_functions.glBindVertexArray(0);
        m_functions.glBindBuffer(GL_ARRAY_BUFFER, 0);
        m_functions.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        m_functions.checkError();
    }

    void cleanupMesh() {
        if (m_vao != 0) {
            m_functions.glDeleteVertexArrays(1, &m_vao);
            m_vao = 0;
        }
        if (m_base_quad_vbo != 0) {
            m_functions.glDeleteBuffers(1, &m_base_quad_vbo);
            m_base_quad_vbo = 0;
        }
        if (m_base_quad_ibo != 0) {
            m_functions.glDeleteBuffers(1, &m_base_quad_ibo);
            m_base_quad_ibo = 0;
        }
        if (m_instance_data_vbo != 0) {
            m_functions.glDeleteBuffers(1, &m_instance_data_vbo);
            m_instance_data_vbo = 0;
        }
    }
};

// Static member definitions must be in a .cpp file.
// For now, declaring them here and will move to Meshes.cpp
// const std::vector<BaseQuadVert> InstancedIconArrayMesh::s_base_quad_verts;
// const std::vector<unsigned int> InstancedIconArrayMesh::s_base_quad_indices;


} // namespace Legacy

#undef VOIDPTR_OFFSETOF
#undef VPO
