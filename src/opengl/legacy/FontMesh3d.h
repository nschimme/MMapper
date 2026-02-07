#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "Shaders.h"
#include "SimpleMesh.h"

#include <memory>
#include <optional>
#include <vector>

#define VOIDPTR_OFFSETOF(x, y) reinterpret_cast<void *>(offsetof(x, y))
#define VPO(x) VOIDPTR_OFFSETOF(VertexType_, x)

namespace Legacy {

// Textured mesh with color modulated by color attribute,
// but it does a screen space transform. See FontInstanceData.
template<typename VertexType_>
class NODISCARD SimpleFont3dMesh : public SimpleMesh<VertexType_, FontShader>
{
public:
    using Base = SimpleMesh<VertexType_, FontShader>;

private:
    struct NODISCARD Attribs final
    {
        GLuint basePos = INVALID_ATTRIB_LOCATION;
        GLuint colorPos = INVALID_ATTRIB_LOCATION;
        GLuint rectPos = INVALID_ATTRIB_LOCATION;
        GLuint packedParamsPos = INVALID_ATTRIB_LOCATION;

        static Attribs getLocations(AbstractShaderProgram &fontShader)
        {
            Attribs result;
            result.basePos = fontShader.getAttribLocation("aBase");
            result.colorPos = fontShader.getAttribLocation("aColor");
            result.rectPos = fontShader.getAttribLocation("aRect");
            result.packedParamsPos = fontShader.getAttribLocation("aPacked");
            return result;
        }
    };

private:
    std::optional<Attribs> m_boundAttribs;

public:
    explicit SimpleFont3dMesh(const SharedFunctions &sharedFunctions,
                              const std::shared_ptr<FontShader> &sharedProgram)
        : Base(sharedFunctions, sharedProgram)
    {}

    explicit SimpleFont3dMesh(const SharedFunctions &sharedFunctions,
                              const std::shared_ptr<FontShader> &sharedProgram,
                              const DrawModeEnum mode,
                              const std::vector<VertexType_> &verts)
        : Base(sharedFunctions, sharedProgram, mode, verts)
    {}

private:
    void virt_bind() override
    {
        const auto vertSize = static_cast<GLsizei>(sizeof(VertexType_));
        static_assert(sizeof(std::declval<VertexType_>().base) == 3 * sizeof(GLfloat));
        static_assert(sizeof(std::declval<VertexType_>().color) == 4);
        static_assert(sizeof(std::declval<VertexType_>().offsetX) == 2);
        static_assert(sizeof(std::declval<VertexType_>().packedParams) == 4);

        Functions &gl = Base::m_functions;

        gl.glBindBuffer(GL_ARRAY_BUFFER, Base::m_vbo.get());
        const auto attribs = Attribs::getLocations(Base::m_program);

        gl.enableAttrib(attribs.basePos, 3, GL_FLOAT, GL_FALSE, vertSize, VPO(base));
        gl.enableAttrib(attribs.colorPos, 4, GL_UNSIGNED_BYTE, GL_TRUE, vertSize, VPO(color));
        gl.enableAttrib(attribs.rectPos, 4, GL_SHORT, GL_FALSE, vertSize, VPO(offsetX));
        gl.enableAttribI(attribs.packedParamsPos, 1, GL_UNSIGNED_INT, vertSize, VPO(packedParams));

        gl.glVertexAttribDivisor(attribs.basePos, 1);
        gl.glVertexAttribDivisor(attribs.colorPos, 1);
        gl.glVertexAttribDivisor(attribs.rectPos, 1);
        gl.glVertexAttribDivisor(attribs.packedParamsPos, 1);

        m_boundAttribs = attribs;
    }

    void virt_unbind() override
    {
        if (!m_boundAttribs) {
            assert(false);
            return;
        }
        Functions &gl = Base::m_functions;
        const auto attribs = m_boundAttribs.value();
        gl.glDisableVertexAttribArray(attribs.basePos);
        gl.glDisableVertexAttribArray(attribs.colorPos);
        gl.glDisableVertexAttribArray(attribs.rectPos);
        gl.glDisableVertexAttribArray(attribs.packedParamsPos);

        gl.glVertexAttribDivisor(attribs.basePos, 0);
        gl.glVertexAttribDivisor(attribs.colorPos, 0);
        gl.glVertexAttribDivisor(attribs.rectPos, 0);
        gl.glVertexAttribDivisor(attribs.packedParamsPos, 0);

        gl.glBindBuffer(GL_ARRAY_BUFFER, 0);
        m_boundAttribs.reset();
    }
};

class NODISCARD FontMesh3d final : public SimpleFont3dMesh<FontInstanceData>
{
private:
    using Base = SimpleFont3dMesh<FontInstanceData>;
    MMTextureId m_textureId;

public:
    explicit FontMesh3d(const SharedFunctions &functions,
                        const std::shared_ptr<FontShader> &sharedShader,
                        MMTextureId textureId,
                        DrawModeEnum mode,
                        const std::vector<FontInstanceData> &verts);

public:
    ~FontMesh3d() final;

private:
    NODISCARD bool virt_modifiesRenderState() const final { return true; }
    NODISCARD GLRenderState virt_modifyRenderState(const GLRenderState &renderState) const final;
};

} // namespace Legacy

#undef VOIDPTR_OFFSETOF
#undef VPO
