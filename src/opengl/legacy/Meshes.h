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

        NODISCARD static Attribs getLocations(AbstractShaderProgram &shader)
        {
            Attribs result;
            result.vertPos = shader.getAttribLocation("aVert");
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

template<typename VertexType_>
class NODISCARD MegaRoomMesh final : public SimpleMesh<VertexType_, MegaRoomShader>
{
public:
    using Base = SimpleMesh<VertexType_, MegaRoomShader>;
    using Base::Base;

private:
    struct NODISCARD Attribs final
    {
        GLuint pos = INVALID_ATTRIB_LOCATION;
        GLuint terrain_trail = INVALID_ATTRIB_LOCATION;
        GLuint flags = INVALID_ATTRIB_LOCATION;
        GLuint mob_flags = INVALID_ATTRIB_LOCATION;
        GLuint load_flags = INVALID_ATTRIB_LOCATION;
        GLuint wall_info0 = INVALID_ATTRIB_LOCATION;
        GLuint wall_info1 = INVALID_ATTRIB_LOCATION;
        GLuint wall_info2 = INVALID_ATTRIB_LOCATION;

        NODISCARD static Attribs getLocations(MegaRoomShader &shader)
        {
            Attribs r;
            r.pos = shader.getAttribLocation("aPos");
            r.terrain_trail = shader.getAttribLocation("aTerrainTrail");
            r.flags = shader.getAttribLocation("aFlags");
            r.mob_flags = shader.getAttribLocation("aMobFlags");
            r.load_flags = shader.getAttribLocation("aLoadFlags");
            r.wall_info0 = shader.getAttribLocation("aWallInfo0");
            r.wall_info1 = shader.getAttribLocation("aWallInfo1");
            r.wall_info2 = shader.getAttribLocation("aWallInfo2");
            return r;
        }
    };

    std::optional<Attribs> m_boundAttribs;

    void virt_bind() override
    {
        const auto vertSize = static_cast<GLsizei>(sizeof(VertexType_));
        Functions &gl = this->m_functions;
        const auto attribs = Attribs::getLocations(this->m_program);
        gl.glBindBuffer(GL_ARRAY_BUFFER, this->m_vbo.get());

        gl.enableAttribI(attribs.pos, 3, GL_INT, vertSize, VPO(pos));
        gl.enableAttribI(attribs.terrain_trail, 1, GL_UNSIGNED_INT, vertSize, VPO(terrain_trail));
        gl.enableAttribI(attribs.flags, 1, GL_UNSIGNED_INT, vertSize, VPO(flags));
        gl.enableAttribI(attribs.mob_flags, 1, GL_UNSIGNED_INT, vertSize, VPO(mob_flags));
        gl.enableAttribI(attribs.load_flags, 1, GL_UNSIGNED_INT, vertSize, VPO(load_flags));
        gl.enableAttribI(attribs.wall_info0, 1, GL_UNSIGNED_INT, vertSize, VPO(wall_info[0]));
        gl.enableAttribI(attribs.wall_info1, 1, GL_UNSIGNED_INT, vertSize, VPO(wall_info[1]));
        gl.enableAttribI(attribs.wall_info2, 1, GL_UNSIGNED_INT, vertSize, VPO(wall_info[2]));

        // instancing
        gl.glVertexAttribDivisor(attribs.pos, 1);
        gl.glVertexAttribDivisor(attribs.terrain_trail, 1);
        gl.glVertexAttribDivisor(attribs.flags, 1);
        gl.glVertexAttribDivisor(attribs.mob_flags, 1);
        gl.glVertexAttribDivisor(attribs.load_flags, 1);
        gl.glVertexAttribDivisor(attribs.wall_info0, 1);
        gl.glVertexAttribDivisor(attribs.wall_info1, 1);
        gl.glVertexAttribDivisor(attribs.wall_info2, 1);

        m_boundAttribs = attribs;
    }

    void virt_unbind() override
    {
        if (!m_boundAttribs) {
            assert(false);
            return;
        }

        auto &attribs = m_boundAttribs.value();
        Functions &gl = this->m_functions;
        gl.glDisableVertexAttribArray(attribs.pos);
        gl.glDisableVertexAttribArray(attribs.terrain_trail);
        gl.glDisableVertexAttribArray(attribs.flags);
        gl.glDisableVertexAttribArray(attribs.mob_flags);
        gl.glDisableVertexAttribArray(attribs.load_flags);
        gl.glDisableVertexAttribArray(attribs.wall_info0);
        gl.glDisableVertexAttribArray(attribs.wall_info1);
        gl.glDisableVertexAttribArray(attribs.wall_info2);
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
        static_assert(sizeof(std::declval<VertexType_>().tex) == 3 * sizeof(GLfloat));
        static_assert(sizeof(std::declval<VertexType_>().vert) == 3 * sizeof(GLfloat));

        Functions &gl = Base::m_functions;
        const auto attribs = Attribs::getLocations(Base::m_program);
        gl.glBindBuffer(GL_ARRAY_BUFFER, Base::m_vbo.get());
        gl.enableAttrib(attribs.texPos, 3, GL_FLOAT, GL_FALSE, vertSize, VPO(tex));
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
        static_assert(sizeof(std::declval<VertexType_>().tex) == 3 * sizeof(GLfloat));
        static_assert(sizeof(std::declval<VertexType_>().vert) == 3 * sizeof(GLfloat));

        Functions &gl = Base::m_functions;
        const auto attribs = Attribs::getLocations(Base::m_program);
        gl.glBindBuffer(GL_ARRAY_BUFFER, Base::m_vbo.get());
        gl.enableAttrib(attribs.colorPos, 4, GL_UNSIGNED_BYTE, GL_TRUE, vertSize, VPO(color));
        gl.enableAttrib(attribs.texPos, 3, GL_FLOAT, GL_FALSE, vertSize, VPO(tex));
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

// Textured mesh with color modulated by color attribute.
template<typename VertexType_>
class NODISCARD RoomQuadTexMesh final : public SimpleMesh<VertexType_, RoomQuadTexShader>
{
public:
    using Base = SimpleMesh<VertexType_, RoomQuadTexShader>;
    using Base::Base;

private:
    struct NODISCARD Attribs final
    {
        GLuint vertTexColPos = INVALID_ATTRIB_LOCATION;

        NODISCARD static Attribs getLocations(RoomQuadTexShader &fontShader)
        {
            Attribs result;
            result.vertTexColPos = fontShader.getAttribLocation("aVertTexCol");
            return result;
        }
    };

    std::optional<Attribs> m_boundAttribs;

    void virt_bind() override
    {
        const auto vertSize = static_cast<GLsizei>(sizeof(VertexType_));
        static_assert(sizeof(std::declval<VertexType_>().vertTexCol) == 4 * sizeof(int32_t));

        Functions &gl = Base::m_functions;
        const auto attribs = Attribs::getLocations(Base::m_program);
        gl.glBindBuffer(GL_ARRAY_BUFFER, Base::m_vbo.get());
        // ivec4
        gl.enableAttribI(attribs.vertTexColPos, 4, GL_INT, vertSize, VPO(vertTexCol));

        // instancing
        gl.glVertexAttribDivisor(attribs.vertTexColPos, 1);

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
        gl.glDisableVertexAttribArray(attribs.vertTexColPos);
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
