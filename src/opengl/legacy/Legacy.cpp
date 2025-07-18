// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "Legacy.h"

#include "../../display/Textures.h"
#include "../../global/utils.h"
#include "../OpenGLTypes.h"
#include "AbstractShaderProgram.h"
#include "Binders.h"
#include "FontMesh3d.h"
#include "Meshes.h"
#include "ShaderUtils.h"
#include "Shaders.h"
#include "SimpleMesh.h"
#include "VBO.h"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <QDebug>
#include <QFile>
#include <QMessageLogContext>
#include <QOpenGLExtraFunctions>
#include <QOpenGLTexture>

namespace Legacy {
void Functions::initializeOpenGLFunctions()
{
    if (!m_gl) {
        m_gl.reset(new QOpenGLExtraFunctions(QOpenGLContext::currentContext()));
    }
}

template<template<typename> typename Mesh_, typename VertType_, typename ProgType_>
NODISCARD static auto createMesh(const SharedFunctions &functions,
                                 const DrawModeEnum mode,
                                 const std::vector<VertType_> &batch,
                                 const std::shared_ptr<ProgType_> &prog)
{
    using Mesh = Mesh_<VertType_>;
    static_assert(std::is_same_v<typename Mesh::ProgramType, ProgType_>);
    return std::make_unique<Mesh>(functions, prog, mode, batch);
}

template<template<typename> typename Mesh_, typename VertType_, typename ProgType_>
NODISCARD static UniqueMesh createUniqueMesh(const SharedFunctions &functions,
                                             const DrawModeEnum mode,
                                             const std::vector<VertType_> &batch,
                                             const std::shared_ptr<ProgType_> &prog)
{
    assert(mode != DrawModeEnum::INVALID);
    return UniqueMesh(createMesh<Mesh_, VertType_, ProgType_>(functions, mode, batch, prog));
}

template<template<typename> typename Mesh_, typename VertType_, typename ProgType_>
NODISCARD static UniqueMesh createTexturedMesh(const SharedFunctions &functions,
                                               const DrawModeEnum mode,
                                               const std::vector<VertType_> &batch,
                                               const std::shared_ptr<ProgType_> &prog,
                                               const MMTextureId texture)
{
    assert(static_cast<size_t>(mode) >= VERTS_PER_TRI);
    return UniqueMesh{std::make_unique<TexturedRenderable>(
        texture, createMesh<Mesh_, VertType_, ProgType_>(functions, mode, batch, prog))};
}

UniqueMesh Functions::createPointBatch(const std::vector<ColorVert> &batch)
{
    const auto &prog = getShaderPrograms().getPointShader();
    return Legacy::createUniqueMesh<PointMesh>(shared_from_this(), DrawModeEnum::POINTS, batch, prog);
}

UniqueMesh Functions::createPlainBatch(const DrawModeEnum mode, const std::vector<glm::vec3> &batch)
{
    assert(static_cast<size_t>(mode) >= VERTS_PER_LINE);
    const auto &prog = getShaderPrograms().getPlainUColorShader();
    return Legacy::createUniqueMesh<PlainMesh>(shared_from_this(), mode, batch, prog);
}

UniqueMesh Functions::createColoredBatch(const DrawModeEnum mode,
                                         const std::vector<ColorVert> &batch)
{
    assert(static_cast<size_t>(mode) >= VERTS_PER_LINE);
    const auto &prog = getShaderPrograms().getPlainAColorShader();
    return Legacy::createUniqueMesh<ColoredMesh>(shared_from_this(), mode, batch, prog);
}

UniqueMesh Functions::createTexturedBatch(const DrawModeEnum mode,
                                          const std::vector<TexVert> &batch,
                                          const MMTextureId texture)
{
    assert(static_cast<size_t>(mode) >= VERTS_PER_TRI);
    const auto &prog = getShaderPrograms().getTexturedUColorShader();
    return Legacy::createTexturedMesh<TexturedMesh>(shared_from_this(), mode, batch, prog, texture);
}

UniqueMesh Functions::createColoredTexturedBatch(const DrawModeEnum mode,
                                                 const std::vector<ColoredTexVert> &batch,
                                                 const MMTextureId texture)
{
    assert(static_cast<size_t>(mode) >= VERTS_PER_TRI);
    const auto &prog = getShaderPrograms().getTexturedAColorShader();
    return Legacy::createTexturedMesh<ColoredTexturedMesh>(shared_from_this(),
                                                            mode,
                                                            batch,
                                                            prog,
                                                            texture);
}

template<typename VertexType_, template<typename> typename Mesh_, typename ShaderType_>
static void renderImmediate(const SharedFunctions &sharedFunctions,
                            const DrawModeEnum mode,
                            const std::vector<VertexType_> &verts,
                            const std::shared_ptr<ShaderType_> &sharedShader,
                            const GLRenderState &renderState)
{
    if (verts.empty()) {
        return;
    }

    static WeakVbo weak;
    auto shared = weak.lock();
    if (shared == nullptr) {
        weak = shared = sharedFunctions->getStaticVbos().alloc();
        if (shared == nullptr) {
            throw std::runtime_error("OpenGL error: failed to alloc VBO");
        }
    }

    VBO &vbo = *shared;
    if (!vbo) {
        vbo.emplace(sharedFunctions);
    }

    using Mesh = Mesh_<VertexType_>;
    static_assert(std::is_same_v<typename Mesh::ProgramType, ShaderType_>);

    const auto before = vbo.get();
    {
        Mesh mesh{sharedFunctions, sharedShader};
        {
            // temporarily loan our VBO to the mesh.
            mesh.unsafe_swapVboId(vbo);
            assert(!vbo);
            {
                mesh.setDynamic(mode, verts);
                mesh.render(renderState);
            }
            mesh.unsafe_swapVboId(vbo);
            assert(vbo);
        }
    }
    const auto after = vbo.get();
    assert(before == after);
    sharedFunctions->clearVbo(vbo.get());
}

void Functions::renderPlain(const DrawModeEnum mode,
                            const std::vector<glm::vec3> &verts,
                            const GLRenderState &state)
{
    assert(static_cast<size_t>(mode) >= VERTS_PER_LINE);
    const auto &shared = shared_from_this();
    const auto &prog = getShaderPrograms().getPlainUColorShader();
    Legacy::renderImmediate<glm::vec3, Legacy::PlainMesh>(shared, mode, verts, prog, state);
}

void Functions::renderColored(const DrawModeEnum mode,
                              const std::vector<ColorVert> &verts,
                              const GLRenderState &state)
{
    assert(static_cast<size_t>(mode) >= VERTS_PER_LINE);
    const auto &prog = getShaderPrograms().getPlainAColorShader();
    Legacy::renderImmediate<ColorVert, Legacy::ColoredMesh>(shared_from_this(),
                                                            mode,
                                                            verts,
                                                            prog,
                                                            state);
}

void Functions::renderPoints(const std::vector<ColorVert> &verts, const GLRenderState &state)
{
    assert(state.uniforms.pointSize);
    const auto &prog = getShaderPrograms().getPointShader();
    Legacy::renderImmediate<ColorVert, Legacy::PointMesh>(shared_from_this(),
                                                          DrawModeEnum::POINTS,
                                                          verts,
                                                          prog,
                                                          state);
}

void Functions::renderTextured(const DrawModeEnum mode,
                               const std::vector<TexVert> &verts,
                               const GLRenderState &state)
{
    assert(static_cast<size_t>(mode) >= VERTS_PER_TRI);
    const auto &prog = getShaderPrograms().getTexturedUColorShader();
    Legacy::renderImmediate<TexVert, Legacy::TexturedMesh>(shared_from_this(),
                                                           mode,
                                                           verts,
                                                           prog,
                                                           state);
}

void Functions::renderColoredTextured(const DrawModeEnum mode,
                                      const std::vector<ColoredTexVert> &verts,
                                      const GLRenderState &state)
{
    assert(static_cast<size_t>(mode) >= VERTS_PER_TRI);
    const auto &prog = getShaderPrograms().getTexturedAColorShader();
    Legacy::renderImmediate<ColoredTexVert, Legacy::ColoredTexturedMesh>(shared_from_this(),
                                                                         mode,
                                                                         verts,
                                                                         prog,
                                                                         state);
}

void Functions::renderFont3d(const SharedMMTexture &texture, const std::vector<FontVert3d> &verts)

{
    const auto state = GLRenderState()
                           .withBlend(BlendModeEnum::TRANSPARENCY)
                           .withDepthFunction(std::nullopt)
                           .withTexture0(texture->getId());

    const auto &prog = getShaderPrograms().getFontShader();
    Legacy::renderImmediate<FontVert3d, Legacy::SimpleFont3dMesh>(shared_from_this(),
                                                                  DrawModeEnum::QUADS,
                                                                  verts,
                                                                  prog,
                                                                  state);
}

UniqueMesh Functions::createFontMesh(const SharedMMTexture &texture,
                                     const DrawModeEnum mode,
                                     const std::vector<FontVert3d> &batch)
{
    assert(static_cast<size_t>(mode) >= VERTS_PER_TRI);
    const auto &prog = getShaderPrograms().getFontShader();
    return UniqueMesh{
        std::make_unique<Legacy::FontMesh3d>(shared_from_this(), prog, texture, mode, batch)};
}

Functions::Functions(Badge<Functions>)
    : m_gl{new QOpenGLExtraFunctions(QOpenGLContext::currentContext())}
    , m_shaderPrograms{std::make_unique<ShaderPrograms>(*this)}
    , m_staticVbos{std::make_unique<StaticVbos>()}
    , m_texLookup{std::make_unique<TexLookup>()}
{}

Functions::~Functions()
{
    cleanup();
}

/// <ul>
/// <li>Resets the Wrapped GL's cached copies of (compiled) shaders given out
/// to new meshes. This <em>does NOT</em> expire the shaders belonging to old
/// mesh objects, so that means it's possible to end up with a mixture of old
/// and new mesh objects each with different instances of the same shader
/// program. (In other words: if you want to add shader hot-reloading, then
/// instead of calling this function you'll probably want to just immediately
/// recompile the old shaders.) </li>
///
/// <li>Resets shared pointers to VBOs owned by this object but given out on
/// "extended loan" to static immediate-rendering functions. Those functions
/// only keep static weak pointers to the VBOs, and the weak pointers will
/// expire immediately when you call this function. If you call those
/// functions again, they'll detect the expiration and request new buffers.</li>
/// </ul>
void Functions::cleanup()
{
    if (LOG_VBO_ALLOCATIONS) {
        qInfo() << "Cleanup";
    }

    getShaderPrograms().resetAll();
    getStaticVbos().resetAll();
    getTexLookup().clear();
}

ShaderPrograms &Functions::getShaderPrograms()
{
    return deref(m_shaderPrograms);
}
StaticVbos &Functions::getStaticVbos()
{
    return deref(m_staticVbos);
}

TexLookup &Functions::getTexLookup()
{
    return deref(m_texLookup);
}

std::shared_ptr<Functions> Functions::alloc()
{
    return std::make_shared<Functions>(Badge<Functions>{});
}

/// This only exists so we can detect errors in contexts that don't support \c glDebugMessageCallback().
void Functions::checkError()
{
#define CASE(x) \
    case (x): \
        qCritical() << "OpenGL error" << #x; \
        break;

    bool fail = false;
    while (true) {
        const auto err = m_gl->glGetError();
        if (err == GL_NO_ERROR) {
            break;
        }

        fail = true;

        switch (err) {
            CASE(GL_INVALID_ENUM)
            CASE(GL_INVALID_VALUE)
            CASE(GL_INVALID_OPERATION)
            CASE(GL_OUT_OF_MEMORY)
        default:
            qCritical() << "OpenGL error" << err;
            break;
        }
    }

    if (fail) {
        std::abort();
    }

#undef CASE
}
void Functions::glAttachShader(GLuint program, GLuint shader)
{
    m_gl->glAttachShader(program, shader);
}

void Functions::glBindBuffer(GLenum target, GLuint buffer)
{
    m_gl->glBindBuffer(target, buffer);
}

void Functions::glBlendFunc(GLenum sfactor, GLenum dfactor)
{
    m_gl->glBlendFunc(sfactor, dfactor);
}

void Functions::glBlendFuncSeparate(GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha, GLenum dstAlpha)
{
    m_gl->glBlendFuncSeparate(srcRGB, dstRGB, srcAlpha, dstAlpha);
}

void Functions::glBufferData(GLenum target, GLsizeiptr size, const void *data, GLenum usage)
{
    m_gl->glBufferData(target, size, data, usage);
}

void Functions::glClear(GLbitfield mask)
{
    m_gl->glClear(mask);
}

void Functions::glClearColor(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha)
{
    m_gl->glClearColor(red, green, blue, alpha);
}

void Functions::glCompileShader(GLuint shader)
{
    m_gl->glCompileShader(shader);
}

GLuint Functions::glCreateProgram()
{
    return m_gl->glCreateProgram();
}

GLuint Functions::glCreateShader(GLenum type)
{
    return m_gl->glCreateShader(type);
}

void Functions::glCullFace(GLenum mode)
{
    m_gl->glCullFace(mode);
}

void Functions::glDeleteBuffers(GLsizei n, const GLuint *buffers)
{
    m_gl->glDeleteBuffers(n, buffers);
}

void Functions::glDeleteProgram(GLuint program)
{
    m_gl->glDeleteProgram(program);
}

void Functions::glDeleteShader(GLuint shader)
{
    m_gl->glDeleteShader(shader);
}

void Functions::glDepthFunc(GLenum func)
{
    m_gl->glDepthFunc(func);
}

void Functions::glDetachShader(GLuint program, GLuint shader)
{
    m_gl->glDetachShader(program, shader);
}

void Functions::glDisable(GLenum cap)
{
    m_gl->glDisable(cap);
}

void Functions::glDisableVertexAttribArray(GLuint index)
{
    m_gl->glDisableVertexAttribArray(index);
}

void Functions::glDrawArrays(GLenum mode, GLint first, GLsizei count)
{
    m_gl->glDrawArrays(mode, first, count);
}

void Functions::glEnable(GLenum cap)
{
    m_gl->glEnable(cap);
}

void Functions::glEnableVertexAttribArray(GLuint index)
{
    m_gl->glEnableVertexAttribArray(index);
}

void Functions::glGenBuffers(GLsizei n, GLuint *buffers)
{
    m_gl->glGenBuffers(n, buffers);
}

GLint Functions::glGetAttribLocation(GLuint program, const GLchar *name)
{
    return m_gl->glGetAttribLocation(program, name);
}

void Functions::glGetIntegerv(GLenum pname, GLint *data)
{
    m_gl->glGetIntegerv(pname, data);
}

void Functions::glGetProgramInfoLog(GLuint program,
                                  GLsizei maxLength,
                                  GLsizei *length,
                                  GLchar *infoLog)
{
    m_gl->glGetProgramInfoLog(program, maxLength, length, infoLog);
}

void Functions::glGetProgramiv(GLuint program, GLenum pname, GLint *params)
{
    m_gl->glGetProgramiv(program, pname, params);
}

void Functions::glGetShaderInfoLog(GLuint shader,
                                 GLsizei maxLength,
                                 GLsizei *length,
                                 GLchar *infoLog)
{
    m_gl->glGetShaderInfoLog(shader, maxLength, length, infoLog);
}

void Functions::glGetShaderiv(GLuint shader, GLenum pname, GLint *params)
{
    m_gl->glGetShaderiv(shader, pname, params);
}

const GLubyte *Functions::glGetString(GLenum name)
{
    return m_gl->glGetString(name);
}

GLint Functions::glGetUniformLocation(GLuint program, const GLchar *name)
{
    return m_gl->glGetUniformLocation(program, name);
}

void Functions::glHint(GLenum target, GLenum mode)
{
    m_gl->glHint(target, mode);
}

GLboolean Functions::glIsBuffer(GLuint buffer)
{
    return m_gl->glIsBuffer(buffer);
}

GLboolean Functions::glIsProgram(GLuint program)
{
    return m_gl->glIsProgram(program);
}

GLboolean Functions::glIsShader(GLuint shader)
{
    return m_gl->glIsShader(shader);
}

GLboolean Functions::glIsTexture(GLuint texture)
{
    return m_gl->glIsTexture(texture);
}

void Functions::glLinkProgram(GLuint program)
{
    m_gl->glLinkProgram(program);
}

void Functions::glShaderSource(GLuint shader,
                             GLsizei count,
                             const GLchar *const *string,
                             const GLint *length)
{
    m_gl->glShaderSource(shader, count, (const char **)string, length);
}

void Functions::glUniform1fv(GLint location, GLsizei count, const GLfloat *value)
{
    m_gl->glUniform1fv(location, count, value);
}

void Functions::glUniform1iv(GLint location, GLsizei count, const GLint *value)
{
    m_gl->glUniform1iv(location, count, value);
}

void Functions::glUniform4fv(GLint location, GLsizei count, const GLfloat *value)
{
    m_gl->glUniform4fv(location, count, value);
}

void Functions::glUniform4iv(GLint location, GLsizei count, const GLint *value)
{
    m_gl->glUniform4iv(location, count, value);
}

void Functions::glUniformMatrix4fv(GLint location,
                                 GLsizei count,
                                 GLboolean transpose,
                                 const GLfloat *value)
{
    m_gl->glUniformMatrix4fv(location, count, transpose, value);
}

void Functions::glUseProgram(GLuint program)
{
    m_gl->glUseProgram(program);
}

void Functions::glVertexAttribPointer(GLuint index,
                                    GLint size,
                                    GLenum type,
                                    GLboolean normalized,
                                    GLsizei stride,
                                    const void *pointer)
{
    m_gl->glVertexAttribPointer(index, size, type, normalized, stride, pointer);
}

void Functions::glGenVertexArrays(GLsizei n, GLuint *arrays)
{
    m_gl->glGenVertexArrays(n, arrays);
}

void Functions::glBindVertexArray(GLuint array)
{
    m_gl->glBindVertexArray(array);
}

void Functions::glDeleteVertexArrays(GLsizei n, const GLuint *arrays)
{
    m_gl->glDeleteVertexArrays(n, arrays);
}

void Functions::glLineWidth(GLfloat lineWidth)
{
    m_gl->glLineWidth(lineWidth);
}

void Functions::glViewport(GLint x, GLint y, GLsizei width, GLsizei height)
{
    m_viewport = Viewport{{x, y}, {width, height}};
    m_gl->glViewport(scalei(x), scalei(y), scalei(width), scalei(height));
}

} // namespace Legacy
