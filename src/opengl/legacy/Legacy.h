#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "../../global/Badge.h"
#include "../../global/RuleOf5.h"
#include "../../global/utils.h"
#include "../OpenGLTypes.h"
#include "AbstractShaderProgram.h" // Added for INVALID_ATTRIB_LOCATION
#include "LegacyTypes.h"           // Added for SharedFunctions, WeakFunctions

#include <cmath>
#include <memory>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <QOpenGLFunctions>

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

// class Functions; // Moved to LegacyTypes.h
// using SharedFunctions = std::shared_ptr<Functions>; // Moved to LegacyTypes.h
// using WeakFunctions = std::weak_ptr<Functions>; // Moved to LegacyTypes.h

/// \c Legacy::Functions implements both GL 2.0 and ES 2.0 (based on a subset of
/// GL 2.0); this is accomplished by using separate implementation files for the
/// differences between GL 2.0 and ES 2.0.
class NODISCARD Functions final : private QOpenGLFunctions,
                                  public std::enable_shared_from_this<Functions>
{
private:
    using Base = QOpenGLFunctions;
    glm::mat4 m_viewProj = glm::mat4(1);
    Viewport m_viewport;
    float m_devicePixelRatio = 1.f;
    std::unique_ptr<ShaderPrograms> m_shaderPrograms;
    std::unique_ptr<StaticVbos> m_staticVbos;
    std::unique_ptr<TexLookup> m_texLookup;

    // FBO for multisampling
    GLuint m_msaaFbo = 0;
    GLuint m_msaaColorBuffer = 0;
    GLuint m_msaaDepthStencilBuffer = 0;
    GLsizei m_msaaWidth = 0;
    GLsizei m_msaaHeight = 0;
    GLsizei m_msaaSamples = 0;

public:
    NODISCARD static std::shared_ptr<Functions> alloc();

public:
    explicit Functions(Badge<Functions>);

    ~Functions();
    DELETE_CTORS_AND_ASSIGN_OPS(Functions);

public:
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
    using Base::initializeOpenGLFunctions;

public:
    using Base::glAttachShader;
    using Base::glBindBuffer;
    using Base::glBlendFunc;
    using Base::glBlendFuncSeparate;
    using Base::glBufferData;
    using Base::glClear;
    using Base::glClearColor;
    using Base::glCompileShader;
    using Base::glCreateProgram;
    using Base::glCreateShader;
    using Base::glCullFace;
    using Base::glDeleteBuffers;
    using Base::glDeleteProgram;
    using Base::glDeleteShader;
    using Base::glDepthFunc;
    using Base::glDetachShader;
    using Base::glDisable;
    using Base::glDisableVertexAttribArray;
    using Base::glDrawArrays;
    using Base::glEnable;
    using Base::glEnableVertexAttribArray;
    using Base::glGenBuffers;
    using Base::glGetAttribLocation;
    using Base::glGetIntegerv;
    using Base::glGetProgramInfoLog;
    using Base::glGetProgramiv;
    using Base::glGetShaderInfoLog;
    using Base::glGetShaderiv;
    using Base::glGetString;
    using Base::glGetUniformLocation;
    using Base::glHint;
    using Base::glLinkProgram;
    using Base::glShaderSource;
    using Base::glUniform1fv;
    using Base::glUniform1iv;
    using Base::glUniform4fv;
    using Base::glUniform4iv;
    using Base::glUniformMatrix4fv;
    using Base::glUseProgram;
    using Base::glVertexAttribPointer;

public:
    // OpenGL man page says "Only width 1 is guaranteed to be supported."
    void glLineWidth(const GLfloat lineWidth)
    {
        // For OpenGL Core Profile, only a line width of 1.0 is guaranteed to be supported.
        // Actual thick lines are expected to be rendered using other techniques
        // (e.g., geometry shaders outputting triangle strips).
        // The input 'lineWidth' parameter might still be used by the geometry shader
        // via uniforms, but the GL state for glLineWidth should be 1.0.

        // Suppress unused parameter warning if lineWidth is truly not used anywhere else now.
        // (void)lineWidth; // Example if you want to be explicit about not using it.
        // However, scalef(lineWidth) might have been logged or used for other checks,
        // so let's keep it minimal for now and just force 1.0 to the Base call.

        // GLfloat desiredScaledWidth = scalef(lineWidth);
        // if (desiredScaledWidth != 1.0f && desiredScaledWidth > 0.0f) {
            // Potentially log: qWarning("glLineWidth: requested %f, using 1.0 for Core Profile", desiredScaledWidth);
        // }

        Base::glLineWidth(1.0f);
    }

public:
    void glViewport(const GLint x, const GLint y, const GLsizei width, const GLsizei height)
    {
        m_viewport = Viewport{{x, y}, {width, height}};
        Base::glViewport(scalei(x), scalei(y), scalei(width), scalei(height));
    }

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

private:
    bool createMsaaFBO(GLsizei width, GLsizei height, GLsizei samples);
    void destroyMsaaFBO();

public:
    void bindMsaaFBO();    // Bind the MSAA FBO for rendering
    void resolveMsaaFBO(GLuint targetFramebuffer = 0); // Resolve MSAA FBO to target (default is screen)
    void handleResizeForMsaaFBO(GLsizei newWidth, GLsizei newHeight); // Recreate FBO on resize

public:
    /// platform-specific (ES vs GL)
    NODISCARD static const char *getShaderVersion();

private:
    template<typename VertexType_>
    NODISCARD GLsizei setVbo_internal(const GLuint vbo,
                                      const std::vector<VertexType_> &batch,
                                      const BufferUsageEnum usage)
    {
        const auto numVerts = static_cast<GLsizei>(batch.size());
        const auto vertSize = static_cast<GLsizei>(sizeof(VertexType_));
        const auto numBytes = numVerts * vertSize;
        Base::glBindBuffer(GL_ARRAY_BUFFER, vbo);
        Base::glBufferData(GL_ARRAY_BUFFER, numBytes, batch.data(), Legacy::toGLenum(usage));
        Base::glBindBuffer(GL_ARRAY_BUFFER, 0);
        return numVerts;
    }

public:
    /// platform-specific (ES vs GL)
    NODISCARD static bool canRenderQuads();

    /// platform-specific (ES vs GL)
    NODISCARD static std::optional<GLenum> toGLenum(DrawModeEnum mode);

public:
    void enableAttrib(const GLuint index,
                      const GLint size,
                      const GLenum type,
                      const GLboolean normalized,
                      const GLsizei stride,
                      const GLvoid *const pointer)
    {
        if (index == static_cast<GLuint>(-1) || index == Legacy::INVALID_ATTRIB_LOCATION) {
            // qWarning("Legacy::Functions::enableAttrib: Attempted to enable attribute with invalid index (%u). Skipping.", index);
            return; // Skip enabling this attribute
        }
        Base::glEnableVertexAttribArray(index);
        Base::glVertexAttribPointer(index, size, type, normalized, stride, pointer);
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
        Base::glBindBuffer(GL_ARRAY_BUFFER, vbo);
        Base::glBufferData(GL_ARRAY_BUFFER, 0, nullptr, Legacy::toGLenum(usage));
        Base::glBindBuffer(GL_ARRAY_BUFFER, 0);
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
