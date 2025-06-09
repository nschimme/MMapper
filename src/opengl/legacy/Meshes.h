#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "../../global/utils.h"
#include "Legacy.h"
#include "Shaders.h"
#include "SimpleMesh.h"

#include <optional>

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

// Textured array mesh (uniform color from shader, or white if not set)
template<typename VertexType_ = TexArrayVert, typename ProgType_ = TexturedArrayProgram>
class NODISCARD TexturedArrayMesh final : public SimpleMesh<VertexType_, ProgType_>
{
public:
    using Base = SimpleMesh<VertexType_, ProgType_>;
    using Base::Base;

private:
    struct NODISCARD Attribs final
    {
        GLuint texCoordPos = INVALID_ATTRIB_LOCATION;
        GLuint vertPos = INVALID_ATTRIB_LOCATION;
        GLuint texLayerPos = INVALID_ATTRIB_LOCATION; // New

        NODISCARD static Attribs getLocations(AbstractShaderProgram &shader)
        {
            Attribs result;
            result.texCoordPos = shader.getAttribLocation("aTexCoord");
            result.vertPos = shader.getAttribLocation("aPosition"); // Matches textured_array_acolor.vert
            result.texLayerPos = shader.getAttribLocation("aTexLayer"); // Matches textured_array_acolor.vert
            return result;
        }
    };

    std::optional<Attribs> boundAttribs;

    void virt_bind() override
    {
        const auto vertSize = static_cast<GLsizei>(sizeof(VertexType_));
        static_assert(sizeof(std::declval<VertexType_>().tex_coord) == 2 * sizeof(GLfloat));
        static_assert(sizeof(std::declval<VertexType_>().position) == 3 * sizeof(GLfloat));
        static_assert(sizeof(std::declval<VertexType_>().tex_layer) == 1 * sizeof(GLfloat));

        Functions &gl = Base::m_functions;
        const auto attribs = Attribs::getLocations(Base::m_program);
        gl.glBindBuffer(GL_ARRAY_BUFFER, Base::m_vbo.get());

        gl.enableAttrib(attribs.texCoordPos, 2, GL_FLOAT, GL_FALSE, vertSize, VPO(tex_coord));
        gl.enableAttrib(attribs.vertPos, 3, GL_FLOAT, GL_FALSE, vertSize, VPO(position));
        gl.enableAttrib(attribs.texLayerPos, 1, GL_FLOAT, GL_FALSE, vertSize, VPO(tex_layer)); // New
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
        gl.glDisableVertexAttribArray(attribs.texCoordPos);
        gl.glDisableVertexAttribArray(attribs.vertPos);
        gl.glDisableVertexAttribArray(attribs.texLayerPos); // New
        gl.glBindBuffer(GL_ARRAY_BUFFER, 0);
        boundAttribs.reset();
    }
};

class NODISCARD InstancedIconArrayMesh final : public IRenderable {
public:
    InstancedIconArrayMesh(SharedFunctions funcs, std::shared_ptr<InstancedArrayIconProgram> prog, MMTextureId array_tex_id);
    ~InstancedIconArrayMesh() override;

    void updateInstances(const std::vector<IconInstanceData>& instance_data);
    MMTextureId getTextureId() const { return m_array_texture_id; }

protected:
    // IRenderable overrides
    void virt_clear() override; // Clears instance data
    void virt_reset() override; // Deletes GL resources
    bool virt_isEmpty() const override;
    void virt_render(const GLRenderState& state) override;

private:
    SharedFunctions m_shared_functions;
    Functions &m_functions;
    std::shared_ptr<InstancedArrayIconProgram> m_program;
    MMTextureId m_array_texture_id;

    GLuint m_vao = 0;
    GLuint m_base_quad_vbo = 0;       // For BaseQuadVert
    GLuint m_base_quad_ibo = 0;       // For indices of the base quad
    GLuint m_instance_data_vbo = 0;   // For IconInstanceData

    GLsizei m_instance_count = 0;
    GLsizei m_base_quad_index_count = 0;

    // Static data for the base quad (centered at 0,0, size 1x1)
    // Actual positioning and scaling happens in the vertex shader based on instance data.
    static const std::vector<BaseQuadVert> s_base_quad_verts;
    static const std::vector<unsigned int> s_base_quad_indices;

    void initMesh(); // Helper called by constructor
    void cleanupMesh(); // Helper called by destructor/reset
};

class NODISCARD InstancedIconArrayMesh final : public IRenderable {
public:
    InstancedIconArrayMesh(SharedFunctions funcs, std::shared_ptr<InstancedArrayIconProgram> prog, MMTextureId array_tex_id)
        : m_shared_functions(std::move(funcs)),
          m_functions(deref(m_shared_functions)),
          m_program(std::move(prog)),
          m_array_texture_id(array_tex_id) {
        initMesh();
    }

    ~InstancedIconArrayMesh() override {
        cleanupMesh();
    }

    void updateInstances(const std::vector<IconInstanceData>& instance_data) {
        m_functions.glBindBuffer(GL_ARRAY_BUFFER, m_instance_data_vbo);
        m_functions.glBufferData(GL_ARRAY_BUFFER, instance_data.size() * sizeof(IconInstanceData), instance_data.data(), GL_STREAM_DRAW);
        m_functions.glBindBuffer(GL_ARRAY_BUFFER, 0);
        m_instance_count = static_cast<GLsizei>(instance_data.size());
    }

    MMTextureId getTextureId() const { return m_array_texture_id; }

protected:
    // IRenderable overrides
    void virt_clear() override { // Clears instance data
        updateInstances({});
    }

    void virt_reset() override { // Deletes GL resources
        cleanupMesh();
        m_instance_count = 0;
        m_base_quad_index_count = 0;
    }

    bool virt_isEmpty() const override {
        return m_instance_count == 0;
    }

    void virt_render(const GLRenderState& state) override {
        if (virt_isEmpty()) {
            return;
        }

        m_functions.checkError();
        auto programUnbinder = m_program->bind();

        m_program->setProjectionViewMatrix(m_functions.getProjectionMatrix());

        // Placeholder for icon size: using pointSize from GLRenderState or default 1.0f
        // TODO: Add a dedicated iconSize to GLRenderState::Uniforms
        float icon_size = state.uniforms.pointSize.value_or(1.0f);
        m_program->setIconBaseSize(icon_size);

        if (m_array_texture_id != INVALID_MM_TEXTURE_ID) {
            // Assuming getTexLookup() provides a way to get the QOpenGLTexture or similar
            // and then bind it. The previous TexturedRenderable used state.withTexture0.
            // For direct binding:
            auto& texLookup = m_functions.getTexLookup();
            if (auto sharedTex = texLookup.get(m_array_texture_id)) { // get() returns SharedMMTexture
                 if (auto qGlTex = sharedTex->get()) { // get() on MMTexture returns QOpenGLTexture*
                    qGlTex->bind(0); // Bind to texture unit 0
                 }
            }
            m_program->setTextureSampler(0);
        }

        // Note: GLRenderStateBinder is not used here as uniforms are set directly via m_program
        // However, other states from GLRenderState (like blend, depth) might be needed.
        // For icons, typical state: enable blend, enable depth test, disable culling.
        RAIIEnable blendEnabler(m_functions, GL_BLEND, state.blend != BlendModeEnum::NONE);
        if (state.blend == BlendModeEnum::TRANSPARENCY) {
             m_functions.glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        } // Add other blend modes if necessary

        RAIIEnable depthEnabler(m_functions, GL_DEPTH_TEST, state.depth.has_value());
        if (state.depth.has_value()) {
            m_functions.glDepthFunc(static_cast<GLenum>(state.depth.value()));
        }


        m_functions.glBindVertexArray(m_vao);
        RAIICallback vaoUnbinder([&]() {
            m_functions.glBindVertexArray(0);
            if (m_array_texture_id != INVALID_MM_TEXTURE_ID) {
                 auto& texLookup = m_functions.getTexLookup();
                 if (auto sharedTex = texLookup.get(m_array_texture_id)) {
                     if (auto qGlTex = sharedTex->get()) {
                        qGlTex->release(0); // Release from texture unit 0
                     }
                 }
            }
            m_functions.checkError();
        });

        m_functions.glDrawElementsInstanced(GL_TRIANGLES, m_base_quad_index_count, GL_UNSIGNED_INT, nullptr, m_instance_count);
    }

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
    // These are defined in Meshes.cpp (or will be)
    static const std::vector<BaseQuadVert> s_base_quad_verts;
    static const std::vector<unsigned int> s_base_quad_indices;

    void initMesh() {
        m_functions.glGenVertexArrays(1, &m_vao);
        m_functions.glBindVertexArray(m_vao);

        m_functions.glGenBuffers(1, &m_base_quad_vbo);
        m_functions.glBindBuffer(GL_ARRAY_BUFFER, m_base_quad_vbo);
        m_functions.glBufferData(GL_ARRAY_BUFFER, s_base_quad_verts.size() * sizeof(BaseQuadVert), s_base_quad_verts.data(), GL_STATIC_DRAW);

        m_functions.glGenBuffers(1, &m_base_quad_ibo);
        m_functions.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_base_quad_ibo);
        m_functions.glBufferData(GL_ELEMENT_ARRAY_BUFFER, s_base_quad_indices.size() * sizeof(unsigned int), s_base_quad_indices.data(), GL_STATIC_DRAW);
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
