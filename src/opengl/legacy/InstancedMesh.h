#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2023 The MMapper Authors

#include "../../global/RAII.h"
#include "../OpenGLTypes.h"
#include "AbstractShaderProgram.h"
#include "Binders.h"
#include "Legacy.h"
#include "VAO.h"
#include "VBO.h"

#include <cassert>
#include <memory>
#include <optional>
#include <vector>

namespace Legacy {

template<typename VertexType_, typename InstanceDataType_, typename ProgramType_>
class NODISCARD InstancedMesh : public IRenderable
{
public:
    using ProgramType = ProgramType_;
    static_assert(std::is_base_of_v<AbstractShaderProgram, ProgramType>);

protected:
    const SharedFunctions m_shared_functions;
    Functions &m_functions;
    const std::shared_ptr<ProgramType_> m_shared_program;
    ProgramType_ &m_program;
    VBO m_vbo;
    VBO m_instanceVbo;
    VAO m_vao;
    DrawModeEnum m_drawMode = DrawModeEnum::INVALID;
    GLsizei m_numVerts = 0;
    GLsizei m_numInstances = 0;

public:
    explicit InstancedMesh(SharedFunctions sharedFunctions, std::shared_ptr<ProgramType_> sharedProgram)
        : m_shared_functions{std::move(sharedFunctions)}
        , m_functions{deref(m_shared_functions)}
        , m_shared_program{std::move(sharedProgram)}
        , m_program{deref(m_shared_program)}
    {
        m_vao.emplace(m_shared_functions);
    }

    explicit InstancedMesh(const SharedFunctions &sharedFunctions,
                        const std::shared_ptr<ProgramType_> &sharedProgram,
                        const DrawModeEnum mode,
                        const std::vector<VertexType_> &verts,
                        const std::vector<InstanceDataType_> &instanceData)
        : InstancedMesh{sharedFunctions, sharedProgram}
    {
        setStatic(mode, verts, instanceData);
    }

    ~InstancedMesh() override { reset(); }

protected:
    class NODISCARD AttribUnbinder final
    {
    private:
        InstancedMesh *m_self = nullptr;

    public:
        AttribUnbinder() = delete;
        DEFAULT_MOVES_DELETE_COPIES(AttribUnbinder);

    public:
        explicit AttribUnbinder(InstancedMesh &self)
            : m_self{&self}
        {}
        ~AttribUnbinder()
        {
            if (m_self != nullptr) {
                m_self->unbindAttribs();
            }
        }
    };

    NODISCARD AttribUnbinder bindAttribs()
    {
        virt_bind();
        return AttribUnbinder{*this};
    }

private:
    friend AttribUnbinder;
    void unbindAttribs() { virt_unbind(); }
    virtual void virt_bind() = 0;
    virtual void virt_unbind() = 0;

public:
    void setStatic(const DrawModeEnum mode, const std::vector<VertexType_> &verts, const std::vector<InstanceDataType_> &instanceData)
    {
        setCommon(mode, verts, instanceData, BufferUsageEnum::STATIC_DRAW);
    }

private:
    void setCommon(const DrawModeEnum mode,
                   const std::vector<VertexType_> &verts,
                   const std::vector<InstanceDataType_> &instanceData,
                   const BufferUsageEnum usage)
    {
        const auto numVerts = verts.size();
        assert(mode == DrawModeEnum::INVALID || numVerts % static_cast<size_t>(mode) == 0);

        if (!m_vbo && numVerts != 0) {
            m_vbo.emplace(m_shared_functions);
        }

        if (!m_instanceVbo && !instanceData.empty()) {
            m_instanceVbo.emplace(m_shared_functions);
        }

        if (m_vbo) {
            auto tmp = m_functions.setVbo(mode, m_vbo.get(), verts, usage);
            m_drawMode = tmp.first;
            m_numVerts = tmp.second;
        } else {
            m_drawMode = DrawModeEnum::INVALID;
            m_numVerts = 0;
        }

        if (m_instanceVbo) {
            m_numInstances = static_cast<GLsizei>(instanceData.size());
            m_functions.glBindBuffer(GL_ARRAY_BUFFER, m_instanceVbo.get());
            m_functions.glBufferData(GL_ARRAY_BUFFER, instanceData.size() * sizeof(InstanceDataType_), instanceData.data(), Legacy::toGLenum(usage));
            m_functions.glBindBuffer(GL_ARRAY_BUFFER, 0);
        } else {
            m_numInstances = 0;
        }
    }

private:
    void virt_clear() final
    {
        if (m_drawMode != DrawModeEnum::INVALID) {
            setStatic(m_drawMode, {}, {});
        }
        assert(isEmpty());
    }

    void virt_reset() final
    {
        m_vao.reset();
        m_drawMode = DrawModeEnum::INVALID;
        m_numVerts = 0;
        m_numInstances = 0;
        m_vbo.reset();
        m_instanceVbo.reset();
        assert(isEmpty() && !m_vbo && !m_instanceVbo);
    }

private:
    NODISCARD bool virt_isEmpty() const final
    {
        return !m_vbo || m_numVerts == 0 || m_numInstances == 0 || m_drawMode == DrawModeEnum::INVALID;
    }

private:
    void virt_render(const GLRenderState &renderState) final
    {
        if (isEmpty()) {
            return;
        }

        m_functions.checkError();

        const glm::mat4 mvp = m_functions.getProjectionMatrix();
        auto programUnbinder = m_program.bind();
        m_program.setUniforms(mvp, renderState.uniforms);
        RenderStateBinder renderStateBinder(m_functions, m_functions.getTexLookup(), renderState);

        m_functions.glBindVertexArray(m_vao.get());
        RAIICallback vaoUnbinder([&]() {
            m_functions.glBindVertexArray(0);
            m_functions.checkError();
        });

        auto attribUnbinder = bindAttribs();

        if (const std::optional<GLenum> &optMode = m_functions.toGLenum(m_drawMode)) {
            m_functions.glDrawArraysInstanced(optMode.value(), 0, m_numVerts, m_numInstances);
        } else {
            assert(false);
        }
    }
};

} // namespace Legacy
