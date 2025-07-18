#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "../../global/Badge.h"
#include "../../global/RuleOf5.h"
#include "../../global/utils.h"
#include "../OpenGLTypes.h"

#include <cmath>
#include <memory>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <QOpenGLExtraFunctions>

class OpenGL;

namespace Legacy {

class StaticVbos;
struct ShaderPrograms;
struct PointSizeBinder;

NODISCARD static inline GLenum toGLenum(const BufferUsageEnum usage)
{
    switch (usage) {
    case BufferUsageEnum::DYNAMIC_DRAW:
        return GL_DYNAMIC_DRAW;
    case BufferUsageEnum::STATIC_DRAW:
        return GL_STATIC_DRAW;
    }
    std::abort();
}

// REVISIT: Find this a new home when there's more than one OpenGL implementation.
// Note: This version is only suitable for drawArrays(). You'll need another function
// to transform indices if you want to use it with drawElements().
template<typename VertexType_>
NODISCARD static inline std::vector<VertexType_> convertQuadsToTris(
    const std::vector<VertexType_> &quads)
{
    // d-c
    // |/|
    // a-b
    const static constexpr int TRIANGLE_VERTS_PER_QUAD = 6;
    const size_t numQuads = quads.size() / VERTS_PER_QUAD;
    const size_t expected = numQuads * TRIANGLE_VERTS_PER_QUAD;
    std::vector<VertexType_> triangles;
    triangles.reserve(expected);
    const auto *it = quads.data();
    for (size_t i = 0; i < numQuads; ++i) {
        const auto &a = *(it++);
        const auto &b = *(it++);
        const auto &c = *(it++);
        const auto &d = *(it++);
        triangles.emplace_back(a);
        triangles.emplace_back(b);
        triangles.emplace_back(c);
        triangles.emplace_back(c);
        triangles.emplace_back(d);
        triangles.emplace_back(a);
    }
    assert(triangles.size() == expected);
    return triangles;
}

class Functions;
using SharedFunctions = std::shared_ptr<Functions>;
using WeakFunctions = std::weak_ptr<Functions>;

/// \c Legacy::Functions implements both GL 3.1 and ES 2.0 (based on a subset of
/// GL 2.0); this is accomplished by using separate implementation files for the
/// differences between GL 3.1 and ES 2.0.
class NODISCARD Functions final : public std::enable_shared_from_this<Functions>
{
private:
    std::unique_ptr<QOpenGLExtraFunctions> m_gl;
    glm::mat4 m_viewProj = glm::mat4(1);
    Viewport m_viewport;
    float m_devicePixelRatio = 1.f;
    std::unique_ptr<ShaderPrograms> m_shaderPrograms;
    std::unique_ptr<StaticVbos> m_staticVbos;
    std::unique_ptr<TexLookup> m_texLookup;
    std::vector<std::shared_ptr<IRenderable>> m_staticMeshes;

public:
    NODISCARD static std::shared_ptr<Functions> alloc();

public:
    explicit Functions(Badge<Functions>);

    ~Functions();
    DELETE_CTORS_AND_ASSIGN_OPS(Functions);

public:
    NODISCARD QOpenGLExtraFunctions *get() { return m_gl.get(); }
    NODISCARD float getDevicePixelRatio() const { return m_devicePixelRatio; }

    void setDevicePixelRatio(const float devicePixelRatio)
    {
        constexpr float ratio = 64.f;
        constexpr float inv_ratio = 1.f / ratio;
        if (!std::isfinite(devicePixelRatio) || !isClamped(devicePixelRatio, inv_ratio, ratio)) {
            throw std::invalid_argument("devicePixelRatio");
        }
        m_devicePixelRatio = devicePixelRatio;
    }

public:
    // The purpose of this function is to safely manage the lifetime of reused meshes
    // like the full screen quad mesh. Caller is expected to only keep a weak pointer
    // to the mesh. See OpenGL::renderPlainFullScreenQuad().
    void addSharedMesh(Badge<OpenGL>, std::shared_ptr<IRenderable> mesh)
    {
        m_staticMeshes.emplace_back(std::move(mesh));
    }

public:
    void initializeOpenGLFunctions();

public:
    // Forward declarations for the functions we need
    void glAttachShader(GLuint program, GLuint shader);
    void glBindBuffer(GLenum target, GLuint buffer);
    void glBlendFunc(GLenum sfactor, GLenum dfactor);
    void glBlendFuncSeparate(GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha, GLenum dstAlpha);
    void glBufferData(GLenum target, GLsizeiptr size, const void *data, GLenum usage);
    void glClear(GLbitfield mask);
    void glClearColor(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
    void glCompileShader(GLuint shader);
    GLuint glCreateProgram();
    GLuint glCreateShader(GLenum type);
    void glCullFace(GLenum mode);
    void glDeleteBuffers(GLsizei n, const GLuint *buffers);
    void glDeleteProgram(GLuint program);
    void glDeleteShader(GLuint shader);
    void glDepthFunc(GLenum func);
    void glDetachShader(GLuint program, GLuint shader);
    void glDisable(GLenum cap);
    void glDisableVertexAttribArray(GLuint index);
    void glDrawArrays(GLenum mode, GLint first, GLsizei count);
    void glEnable(GLenum cap);
    void glEnableVertexAttribArray(GLuint index);
    void glGenBuffers(GLsizei n, GLuint *buffers);
    GLint glGetAttribLocation(GLuint program, const GLchar *name);
    void glGetIntegerv(GLenum pname, GLint *data);
    void glGetProgramInfoLog(GLuint program, GLsizei maxLength, GLsizei *length, GLchar *infoLog);
    void glGetProgramiv(GLuint program, GLenum pname, GLint *params);
    void glGetShaderInfoLog(GLuint shader, GLsizei maxLength, GLsizei *length, GLchar *infoLog);
    void glGetShaderiv(GLuint shader, GLenum pname, GLint *params);
    const GLubyte *glGetString(GLenum name);
    GLint glGetUniformLocation(GLuint program, const GLchar *name);
    void glHint(GLenum target, GLenum mode);
    GLboolean glIsBuffer(GLuint buffer);
    GLboolean glIsProgram(GLuint program);
    GLboolean glIsShader(GLuint shader);
    GLboolean glIsTexture(GLuint texture);
    void glLinkProgram(GLuint program);
    void glShaderSource(GLuint shader,
                        GLsizei count,
                        const GLchar *const *string,
                        const GLint *length);
    void glUniform1fv(GLint location, GLsizei count, const GLfloat *value);
    void glUniform1iv(GLint location, GLsizei count, const GLint *value);
    void glUniform4fv(GLint location, GLsizei count, const GLfloat *value);
    void glUniform4iv(GLint location, GLsizei count, const GLint *value);
    void glUniformMatrix4fv(GLint location,
                            GLsizei count,
                            GLboolean transpose,
                            const GLfloat *value);
    void glUseProgram(GLuint program);
    void glVertexAttribPointer(GLuint index,
                               GLint size,
                               GLenum type,
                               GLboolean normalized,
                               GLsizei stride,
                               const void *pointer);

    // VAO functions
    void glGenVertexArrays(GLsizei n, GLuint *arrays);
    void glBindVertexArray(GLuint array);
    void glDeleteVertexArrays(GLsizei n, const GLuint *arrays);

public:
    void glLineWidth(GLfloat lineWidth);

public:
    void glViewport(GLint x, GLint y, GLsizei width, GLsizei height);

private:
    NODISCARD float scalef(const float f) const
    {
        static_assert(std::is_same_v<float, GLfloat>);
        return f * m_devicePixelRatio;
    }

    NODISCARD int scalei(const int n) const
    {
        static_assert(std::is_same_v<int, GLint>);
        static_assert(std::is_same_v<int, GLsizei>);
        return static_cast<int>(std::lround(scalef(static_cast<float>(n))));
    }

public:
    NODISCARD Viewport getViewport() const { return m_viewport; }

    NODISCARD Viewport getPhysicalViewport() const
    {
        const glm::ivec2 &offset = m_viewport.offset;
        const glm::ivec2 &size = m_viewport.size;
        return Viewport{{scalei(offset.x), scalei(offset.y)}, {scalei(size.x), scalei(size.y)}};
    }

public:
    NODISCARD glm::mat4 getProjectionMatrix() const { return m_viewProj; }

    void setProjectionMatrix(const glm::mat4 &viewProj) { m_viewProj = viewProj; }

public:
    void cleanup();

    NODISCARD ShaderPrograms &getShaderPrograms();

    NODISCARD StaticVbos &getStaticVbos();

    NODISCARD TexLookup &getTexLookup();

private:
    friend PointSizeBinder;
    /// platform-specific (ES vs GL)
    void enableProgramPointSize(bool enable);

private:
    friend OpenGL;
    /// platform-specific (ES vs GL)
    NODISCARD bool tryEnableMultisampling(int requestedSamples);

public:
    /// platform-specific (ES vs GL)
    NODISCARD const char *getShaderVersion();

private:
    template<typename VertexType_>
    NODISCARD GLsizei setVbo_internal(const GLuint vbo,
                                      const std::vector<VertexType_> &batch,
                                      const BufferUsageEnum usage)
    {
        const auto numVerts = static_cast<GLsizei>(batch.size());
        const auto vertSize = static_cast<GLsizei>(sizeof(VertexType_));
        const auto numBytes = numVerts * vertSize;
        m_gl->glBindBuffer(GL_ARRAY_BUFFER, vbo);
        m_gl->glBufferData(GL_ARRAY_BUFFER, numBytes, batch.data(), Legacy::toGLenum(usage));
        m_gl->glBindBuffer(GL_ARRAY_BUFFER, 0);
        return numVerts;
    }

public:
    /// platform-specific (ES vs GL)
    NODISCARD bool canRenderQuads();

    /// platform-specific (ES vs GL)
    NODISCARD std::optional<GLenum> toGLenum(DrawModeEnum mode);

public:
    void enableAttrib(const GLuint index,
                      const GLint size,
                      const GLenum type,
                      const GLboolean normalized,
                      const GLsizei stride,
                      const GLvoid *const pointer)
    {
        m_gl->glEnableVertexAttribArray(index);
        m_gl->glVertexAttribPointer(index, size, type, normalized, stride, pointer);
    }

    template<typename T>
    NODISCARD std::pair<DrawModeEnum, GLsizei> setVbo(
        const DrawModeEnum mode,
        const GLuint vbo,
        const std::vector<T> &batch,
        const BufferUsageEnum usage = BufferUsageEnum::DYNAMIC_DRAW)
    {
        if (mode == DrawModeEnum::QUADS && !canRenderQuads()) {
            return std::pair(DrawModeEnum::TRIANGLES,
                             setVbo_internal(vbo, convertQuadsToTris(batch), usage));
        }
        return std::pair(mode, setVbo_internal(vbo, batch, usage));
    }

    void clearVbo(const GLuint vbo, const BufferUsageEnum usage = BufferUsageEnum::DYNAMIC_DRAW)
    {
        m_gl->glBindBuffer(GL_ARRAY_BUFFER, vbo);
        m_gl->glBufferData(GL_ARRAY_BUFFER, 0, nullptr, Legacy::toGLenum(usage));
        m_gl->glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

public:
    NODISCARD UniqueMesh createPointBatch(const std::vector<ColorVert> &batch);

public:
    NODISCARD UniqueMesh createPlainBatch(DrawModeEnum mode, const std::vector<glm::vec3> &batch);
    NODISCARD UniqueMesh createColoredBatch(DrawModeEnum mode, const std::vector<ColorVert> &batch);
    NODISCARD UniqueMesh createTexturedBatch(DrawModeEnum mode,
                                             const std::vector<TexVert> &batch,
                                             MMTextureId texture);
    NODISCARD UniqueMesh createColoredTexturedBatch(DrawModeEnum mode,
                                                    const std::vector<ColoredTexVert> &batch,
                                                    MMTextureId texture);

public:
    NODISCARD UniqueMesh createFontMesh(const SharedMMTexture &texture,
                                        DrawModeEnum mode,
                                        const std::vector<FontVert3d> &batch);

public:
    void renderPoints(const std::vector<ColorVert> &verts, const GLRenderState &state);

public:
    void renderPlain(DrawModeEnum mode,
                     const std::vector<glm::vec3> &verts,
                     const GLRenderState &state);
    void renderColored(DrawModeEnum mode,
                       const std::vector<ColorVert> &verts,
                       const GLRenderState &state);
    void renderTextured(DrawModeEnum mode,
                        const std::vector<TexVert> &verts,
                        const GLRenderState &state);
    void renderColoredTextured(DrawModeEnum mode,
                               const std::vector<ColoredTexVert> &verts,
                               const GLRenderState &state);
    void renderFont3d(const SharedMMTexture &texture, const std::vector<FontVert3d> &verts);

public:
    void checkError();
};
} // namespace Legacy
