#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "../../global/utils.h"
#include "Legacy.h"
#include "Shaders.h"
#include "SimpleMesh.h"
#include "InstancedMesh.h"

#include <optional>

#define VOIDPTR_OFFSETOF(x, y) reinterpret_cast<void *>(offsetof(x, y))
#define VPO(x) VOIDPTR_OFFSETOF(VertexType_, x)

namespace Legacy {

// Textured instanced mesh with color modulated by uniform
template<typename VertexType_, typename InstanceDataType_>
class NODISCARD InstancedTexturedMesh final : public InstancedMesh<VertexType_, InstanceDataType_, UColorTexturedShader>
{
public:
    using Base = InstancedMesh<VertexType_, InstanceDataType_, UColorTexturedShader>;
    using Base::Base;

private:
    struct NODISCARD Attribs final
    {
        GLuint texPos = INVALID_ATTRIB_LOCATION;
        GLuint vertPos = INVALID_ATTRIB_LOCATION;
        GLuint instanceMatrix = INVALID_ATTRIB_LOCATION;

        NODISCARD static Attribs getLocations(AbstractShaderProgram &fontShader)
        {
            Attribs result;
            result.texPos = fontShader.getAttribLocation("aTexCoord");
            result.vertPos = fontShader.getAttribLocation("aVert");
            result.instanceMatrix = fontShader.getAttribLocation("aInstanceMatrix");
            return result;
        }
    };

    std::optional<Attribs> m_boundAttribs;

    void virt_bind() override
    {
        const auto vertSize = static_cast<GLsizei>(sizeof(VertexType_));
        static_assert(sizeof(std::declval<VertexType_>().tex) == 2 * sizeof(GLfloat));
        static_assert(sizeof(std::declval<VertexType_>().vert) == 3 * sizeof(GLfloat));

        Functions &gl = Base::m_functions;
        const auto attribs = Attribs::getLocations(Base::m_program);
        gl.glBindBuffer(GL_ARRAY_BUFFER, Base::m_vbo.get());
        gl.enableAttrib(attribs.texPos, 2, GL_FLOAT, GL_FALSE, vertSize, VPO(tex));
        gl.enableAttrib(attribs.vertPos, 3, GL_FLOAT, GL_FALSE, vertSize, VPO(vert));

        gl.glBindBuffer(GL_ARRAY_BUFFER, Base::m_instanceVbo.get());
        for (int i = 0; i < 4; ++i) {
            gl.enableAttrib(attribs.instanceMatrix + i, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (void*)(sizeof(glm::vec4) * i));
            gl.glVertexAttribDivisor(attribs.instanceMatrix + i, 1);
        }
        m_boundAttribs = attribs;
    }

    void virt_unbind() override
    {
        if (!m_boundAttribs) {
            assert(false);
            return;
        }

        auto &attribs = m_boundAttribs.value();
        Functions &gl = Base::m_functions;
        gl.glDisableVertexAttribArray(attribs.texPos);
        gl.glDisableVertexAttribArray(attribs.vertPos);
        for (int i = 0; i < 4; ++i) {
            gl.glDisableVertexAttribArray(attribs.instanceMatrix + i);
        }
        gl.glBindBuffer(GL_ARRAY_BUFFER, 0);
        m_boundAttribs.reset();
    }
};

// Textured instanced mesh with per-instance color
template<typename VertexType_, typename InstanceDataType_>
class NODISCARD InstancedColoredTexturedMesh final : public InstancedMesh<VertexType_, InstanceDataType_, AColorTexturedShader>
{
public:
    using Base = InstancedMesh<VertexType_, InstanceDataType_, AColorTexturedShader>;
    using Base::Base;

private:
    struct NODISCARD Attribs final
    {
        GLuint texPos = INVALID_ATTRIB_LOCATION;
        GLuint vertPos = INVALID_ATTRIB_LOCATION;
        GLuint instanceMatrix = INVALID_ATTRIB_LOCATION;
        GLuint instanceColor = INVALID_ATTRIB_LOCATION;

        NODISCARD static Attribs getLocations(AbstractShaderProgram &fontShader)
        {
            Attribs result;
            result.texPos = fontShader.getAttribLocation("aTexCoord");
            result.vertPos = fontShader.getAttribLocation("aVert");
            result.instanceMatrix = fontShader.getAttribLocation("aInstanceMatrix");
            result.instanceColor = fontShader.getAttribLocation("aInstanceColor");
            return result;
        }
    };

    std::optional<Attribs> m_boundAttribs;

    void virt_bind() override
    {
        const auto vertSize = static_cast<GLsizei>(sizeof(VertexType_));
        static_assert(sizeof(std::declval<VertexType_>().tex) == 2 * sizeof(GLfloat));
        static_assert(sizeof(std::declval<VertexType_>().vert) == 3 * sizeof(GLfloat));

        Functions &gl = Base::m_functions;
        const auto attribs = Attribs::getLocations(Base::m_program);
        gl.glBindBuffer(GL_ARRAY_BUFFER, Base::m_vbo.get());
        gl.enableAttrib(attribs.texPos, 2, GL_FLOAT, GL_FALSE, vertSize, VPO(tex));
        gl.enableAttrib(attribs.vertPos, 3, GL_FLOAT, GL_FALSE, vertSize, VPO(vert));

        gl.glBindBuffer(GL_ARRAY_BUFFER, Base::m_instanceVbo.get());
        const auto instanceSize = static_cast<GLsizei>(sizeof(InstanceDataType_));
        gl.enableAttrib(attribs.instanceColor, 4, GL_FLOAT, GL_FALSE, instanceSize, VPO(color));
        gl.glVertexAttribDivisor(attribs.instanceColor, 1);

        for (int i = 0; i < 4; ++i) {
            gl.enableAttrib(attribs.instanceMatrix + i, 4, GL_FLOAT, GL_FALSE, instanceSize, (void*)(offsetof(InstanceDataType_, matrix) + sizeof(glm::vec4) * i));
            gl.glVertexAttribDivisor(attribs.instanceMatrix + i, 1);
        }
        m_boundAttribs = attribs;
    }

    void virt_unbind() override
    {
        if (!m_boundAttribs) {
            assert(false);
            return;
        }

        auto &attribs = m_boundAttribs.value();
        Functions &gl = Base::m_functions;
        gl.glDisableVertexAttribArray(attribs.texPos);
        gl.glDisableVertexAttribArray(attribs.vertPos);
        gl.glDisableVertexAttribArray(attribs.instanceColor);
        for (int i = 0; i < 4; ++i) {
            gl.glDisableVertexAttribArray(attribs.instanceMatrix + i);
        }
        gl.glBindBuffer(GL_ARRAY_BUFFER, 0);
        m_boundAttribs.reset();
    }
};

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

    std::optional<Attribs> m_boundAttribs;

    void virt_bind() override
    {
        static_assert(sizeof(std::declval<VertexType_>()) == 3 * sizeof(GLfloat));

        Functions &gl = Base::m_functions;
        const auto attribs = Attribs::getLocations(Base::m_program);
        gl.glBindBuffer(GL_ARRAY_BUFFER, Base::m_vbo.get());
        gl.enableAttrib(attribs.vertPos, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
        m_boundAttribs = attribs;
    }

    void virt_unbind() override
    {
        if (!m_boundAttribs) {
            assert(false);
            return;
        }

        auto &attribs = m_boundAttribs.value();
        Functions &gl = Base::m_functions;
        gl.glDisableVertexAttribArray(attribs.vertPos);
        gl.glBindBuffer(GL_ARRAY_BUFFER, 0);
        m_boundAttribs.reset();
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

    std::optional<Attribs> m_boundAttribs;

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
        m_boundAttribs = attribs;
    }

    void virt_unbind() override
    {
        if (!m_boundAttribs) {
            assert(false);
            return;
        }

        auto &attribs = m_boundAttribs.value();
        Functions &gl = Base::m_functions;
        gl.glDisableVertexAttribArray(attribs.colorPos);
        gl.glDisableVertexAttribArray(attribs.vertPos);
        gl.glBindBuffer(GL_ARRAY_BUFFER, 0);
        m_boundAttribs.reset();
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

    std::optional<Attribs> m_boundAttribs;

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
        m_boundAttribs = attribs;
    }

    void virt_unbind() override
    {
        if (!m_boundAttribs) {
            assert(false);
            return;
        }

        auto &attribs = m_boundAttribs.value();
        Functions &gl = Base::m_functions;
        gl.glDisableVertexAttribArray(attribs.texPos);
        gl.glDisableVertexAttribArray(attribs.vertPos);
        gl.glBindBuffer(GL_ARRAY_BUFFER, 0);
        m_boundAttribs.reset();
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

    std::optional<Attribs> m_boundAttribs;

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
        m_boundAttribs = attribs;
    }

    void virt_unbind() override
    {
        if (!m_boundAttribs) {
            assert(false);
            return;
        }

        auto &attribs = m_boundAttribs.value();
        Functions &gl = Base::m_functions;
        gl.glDisableVertexAttribArray(attribs.colorPos);
        gl.glDisableVertexAttribArray(attribs.texPos);
        gl.glDisableVertexAttribArray(attribs.vertPos);
        gl.glBindBuffer(GL_ARRAY_BUFFER, 0);
        m_boundAttribs.reset();
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

    std::optional<Attribs> m_boundAttribs;

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
        m_boundAttribs = attribs;
    }

    void virt_unbind() override
    {
        if (!m_boundAttribs) {
            assert(false);
            return;
        }

        auto &attribs = m_boundAttribs.value();
        Functions &gl = Base::m_functions;
        gl.glDisableVertexAttribArray(attribs.colorPos);
        gl.glDisableVertexAttribArray(attribs.vertPos);
        gl.glBindBuffer(GL_ARRAY_BUFFER, 0);
        m_boundAttribs.reset();
    }
};

} // namespace Legacy

#undef VOIDPTR_OFFSETOF
#undef VPO
