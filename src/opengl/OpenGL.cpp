// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Nils Schimmelmann <nschimme@gmail.com> (Jahara)

#include "OpenGL.h"

#include "../display/Textures.h"
#include "../global/ConfigConsts.h"
#include "../global/logging.h"
#include "./legacy/FunctionsES30.h"
#include "./legacy/FunctionsGL33.h"
#include "./legacy/Legacy.h"
#include "./legacy/Meshes.h"
#include "OpenGLConfig.h"
#include "OpenGLProber.h"
#include "OpenGLTypes.h"

#include <cassert>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <QOpenGLContext>
#include <QSurfaceFormat>

OpenGL::OpenGL()
{
    switch (OpenGLConfig::getBackendType()) {
    case OpenGLProber::BackendType::GL:
        m_opengl = Legacy::Functions::alloc<Legacy::FunctionsGL33>();
        break;
    case OpenGLProber::BackendType::GLES:
        m_opengl = Legacy::Functions::alloc<Legacy::FunctionsES30>();
        break;
    case OpenGLProber::BackendType::None:
    default:
        qFatal("Invalid backend type");
        break;
    }
}

OpenGL::~OpenGL() = default;

glm::mat4 OpenGL::getProjectionMatrix() const
{
    return getFunctions().getProjectionMatrix();
}

Viewport OpenGL::getViewport() const
{
    return getFunctions().getViewport();
}

Viewport OpenGL::getPhysicalViewport() const
{
    return getFunctions().getPhysicalViewport();
}

void OpenGL::setProjectionMatrix(const glm::mat4 &m)
{
    getFunctions().setProjectionMatrix(m);
}

void OpenGL::configureFbo(int samples)
{
    getFunctions().configureFbo(samples);
}

void OpenGL::bindFbo()
{
    getFunctions().bindFbo();
}

void OpenGL::releaseFbo()
{
    getFunctions().releaseFbo();
}

void OpenGL::blitFboToDefault()
{
    getFunctions().blitFboToDefault();
}

UniqueMesh OpenGL::createPointBatch(const std::vector<ColorVert> &batch)
{
    return getFunctions().createPointBatch(batch);
}

UniqueMesh OpenGL::createPlainLineBatch(const std::vector<glm::vec3> &batch)
{
    return getFunctions().createPlainBatch(DrawModeEnum::LINES, batch);
}

UniqueMesh OpenGL::createColoredLineBatch(const std::vector<ColorVert> &batch)
{
    return getFunctions().createColoredBatch(DrawModeEnum::LINES, batch);
}

UniqueMesh OpenGL::createPlainTriBatch(const std::vector<glm::vec3> &batch)
{
    return getFunctions().createPlainBatch(DrawModeEnum::TRIANGLES, batch);
}

UniqueMesh OpenGL::createColoredTriBatch(const std::vector<ColorVert> &batch)
{
    return getFunctions().createColoredBatch(DrawModeEnum::TRIANGLES, batch);
}

UniqueMesh OpenGL::createPlainQuadBatch(const std::vector<glm::vec3> &batch)
{
    return getFunctions().createPlainBatch(DrawModeEnum::QUADS, batch);
}

UniqueMesh OpenGL::createColoredQuadBatch(const std::vector<ColorVert> &batch)
{
    return getFunctions().createColoredBatch(DrawModeEnum::QUADS, batch);
}

UniqueMesh OpenGL::createTexturedQuadBatch(const std::vector<TexVert> &batch,
                                           const MMTextureId texture)
{
    return getFunctions().createTexturedBatch(DrawModeEnum::QUADS, batch, texture);
}

UniqueMesh OpenGL::createColoredTexturedQuadBatch(const std::vector<ColoredTexVert> &batch,
                                                  const MMTextureId texture)
{
    return getFunctions().createColoredTexturedBatch(DrawModeEnum::QUADS, batch, texture);
}

UniqueMesh OpenGL::createFontMesh(const SharedMMTexture &texture,
                                  const DrawModeEnum mode,
                                  const std::vector<FontVert3d> &batch)
{
    return getFunctions().createFontMesh(texture, mode, batch);
}

void OpenGL::clear(const Color &color)
{
    const auto v = color.getVec4();
    auto &gl = getFunctions();
    gl.glClearColor(v.r, v.g, v.b, v.a);
    gl.glClear(static_cast<GLbitfield>(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));
}

void OpenGL::clearDepth()
{
    auto &gl = getFunctions();
    gl.glClear(static_cast<GLbitfield>(GL_DEPTH_BUFFER_BIT));
}

void OpenGL::renderPlain(const DrawModeEnum type,
                         const std::vector<glm::vec3> &verts,
                         const GLRenderState &state)
{
    getFunctions().renderPlain(type, verts, state);
}

void OpenGL::renderColored(const DrawModeEnum type,
                           const std::vector<ColorVert> &verts,
                           const GLRenderState &state)
{
    getFunctions().renderColored(type, verts, state);
}

void OpenGL::renderPoints(const std::vector<ColorVert> &verts, const GLRenderState &state)
{
    getFunctions().renderPoints(verts, state);
}

void OpenGL::renderTextured(const DrawModeEnum type,
                            const std::vector<TexVert> &verts,
                            const GLRenderState &state)
{
    getFunctions().renderTextured(type, verts, state);
}

void OpenGL::renderColoredTextured(const DrawModeEnum type,
                                   const std::vector<ColoredTexVert> &verts,
                                   const GLRenderState &state)
{
    getFunctions().renderColoredTextured(type, verts, state);
}

void OpenGL::renderPlainFullScreenQuad(const GLRenderState &renderState)
{
    using MeshType = Legacy::PlainMesh<glm::vec3>;
    static std::weak_ptr<MeshType> g_mesh;
    auto sharedMesh = g_mesh.lock();
    if (sharedMesh == nullptr) {
        if (IS_DEBUG_BUILD) {
            qDebug() << "allocating shared mesh for renderPlainFullScreenQuad";
        }
        auto &sharedFuncs = getSharedFunctions();
        auto &funcs = deref(sharedFuncs);
        g_mesh = sharedMesh
            = std::make_shared<MeshType>(sharedFuncs,
                                         funcs.getShaderPrograms().getPlainUColorShader());
        funcs.addSharedMesh(Badge<OpenGL>{}, sharedMesh);
        MeshType &mesh = deref(sharedMesh);

        // screen is [-1,+1]^3.
        const auto fullScreenQuad = std::vector{glm::vec3{-1, -1, 0},
                                                glm::vec3{+1, -1, 0},
                                                glm::vec3{+1, +1, 0},
                                                glm::vec3{-1, +1, 0}};
        mesh.setStatic(DrawModeEnum::QUADS, fullScreenQuad);
    }

    MeshType &mesh = deref(sharedMesh);
    const auto oldProj = getProjectionMatrix();
    setProjectionMatrix(glm::mat4(1));
    mesh.render(renderState.withDepthFunction(std::nullopt));
    setProjectionMatrix(oldProj);
}

void OpenGL::cleanup()
{
    getFunctions().cleanup();
}

void OpenGL::initializeRenderer(const float devicePixelRatio)
{
    setDevicePixelRatio(devicePixelRatio);

    // REVISIT: Move this somewhere else?
    GLint maxSamples = 0;
    getFunctions().glGetIntegerv(GL_MAX_SAMPLES, &maxSamples);
    OpenGLConfig::setMaxSamples(maxSamples);

    m_rendererInitialized = true;
}

void OpenGL::renderFont3d(const SharedMMTexture &texture, const std::vector<FontVert3d> &verts)
{
    getFunctions().renderFont3d(texture, verts);
}

void OpenGL::initializeOpenGLFunctions()
{
    getFunctions().initializeOpenGLFunctions();
}

const char *OpenGL::glGetString(GLenum name)
{
    return as_cstring(getFunctions().glGetString(name));
}

float OpenGL::getDevicePixelRatio() const
{
    return getFunctions().getDevicePixelRatio();
}

void OpenGL::glViewport(GLint x, GLint y, GLsizei w, GLsizei h)
{
    getFunctions().glViewport(x, y, w, h);
}

void OpenGL::setDevicePixelRatio(const float devicePixelRatio)
{
    getFunctions().setDevicePixelRatio(devicePixelRatio);
}

// NOTE: Technically we could assert that the SharedMMTexture::getId() == id,
// but this is treated as an opaque pointer and we don't include the header for it.
void OpenGL::setTextureLookup(const MMTextureId id, SharedMMTexture tex)
{
    getFunctions().getTexLookup().set(id, std::move(tex));
}

namespace init_array_helper {

NODISCARD static GLint xglGetTexParameteri(Legacy::Functions &gl, GLenum target, GLenum pname)
{
    GLint result = 0;
    gl.glGetTexParameteriv(target, pname, &result);
    return result;
}

NODISCARD static GLint xglGetTexLevelParameteri(Legacy::Functions &gl,
                                                GLenum target,
                                                GLint level,
                                                GLenum pname)
{
    GLint result = 0;
    gl.glGetTexLevelParameteriv(target, level, pname, &result);
    return result;
}

static void buildTexture2dArray(Legacy::Functions &gl,
                                const GLuint vbo,
                                const std::vector<GLuint> &srcNames,
                                const GLuint dstName,
                                const GLsizei img_width,
                                const GLsizei img_height)
{
    assert(vbo != 0);

    if (img_width != img_height)
        throw std::runtime_error("texture must be square");

    using GLsizeui = std::make_unsigned_t<GLsizei>;

    if (!utils::isPowerOfTwo(static_cast<GLsizeui>(img_width)))
        throw std::runtime_error("texture size must be a power of two");

    gl.glBindBuffer(GL_PIXEL_UNPACK_BUFFER, vbo);
    gl.glBufferData(GL_PIXEL_UNPACK_BUFFER, img_width * img_height * 4, nullptr, GL_DYNAMIC_COPY);
    gl.glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

    // GLint oldTexture = xglGetInteger(GL_ACTIVE_TEXTURE);
    // GLint oldTex2d = xglGetInteger(GL_TEXTURE_BINDING_2D);
    // GLint oldTex2dArray = xglGetInteger(GL_TEXTURE_BINDING_2D_ARRAY);
    // GLint oldReadBuffer = xglGetInteger(GL_COPY_READ_BUFFER_BINDING);
    // GLint oldPackBuffer = xglGetInteger(GL_PIXEL_PACK_BUFFER_BINDING);
    // xglGetInteger(GL_PACK_ALIGNMENT);

    gl.glActiveTexture(GL_TEXTURE0);
    gl.glBindTexture(GL_TEXTURE_2D_ARRAY, dstName);
    gl.glActiveTexture(GL_TEXTURE1);
    gl.glBindTexture(GL_TEXTURE_2D, 0);
    gl.glBindBuffer(GL_PIXEL_UNPACK_BUFFER, vbo);
    gl.glBindBuffer(GL_PIXEL_PACK_BUFFER, vbo);
    gl.glPixelStorei(GL_PACK_ALIGNMENT, 4);

    struct NODISCARD CleanupRaii final
    {
        Legacy::Functions &m_gl;
        ~CleanupRaii()
        {
            m_gl.glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
            m_gl.glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
            m_gl.glActiveTexture(GL_TEXTURE1);
            m_gl.glBindTexture(GL_TEXTURE_2D, 0);
            m_gl.glActiveTexture(GL_TEXTURE0);
            m_gl.glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
        }
    };
    CleanupRaii cleanup{gl};

    const GLsizei layers = static_cast<GLsizei>(srcNames.size());
    for (GLsizei layer = 0; layer < layers; ++layer) {
        const GLuint srcName = srcNames[static_cast<GLsizeui>(layer)];

        gl.glActiveTexture(GL_TEXTURE1);
        gl.glBindTexture(GL_TEXTURE_2D, srcName);

        const GLint baseLevel = xglGetTexParameteri(gl, GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL);
        const GLint maxLevel = xglGetTexParameteri(gl, GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL);
        MAYBE_UNUSED const GLint inFmt = xglGetTexLevelParameteri(gl,
                                                                  GL_TEXTURE_2D,
                                                                  0,
                                                                  GL_TEXTURE_INTERNAL_FORMAT);
        if (baseLevel != 0)
            throw std::runtime_error("baseLevel is not zero");

        if (maxLevel != 1000)
            throw std::runtime_error("maxLevel is not 1000");

        {
            const GLint width = xglGetTexLevelParameteri(gl, GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH);
            const GLint height = xglGetTexLevelParameteri(gl, GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT);

            if (img_width != width || img_height != height)
                throw std::runtime_error("input texture is the wrong size");
        }

        for (int level = 0; (img_width >> level) > 0; ++level) {
            gl.glActiveTexture(GL_TEXTURE1);
            const GLint level_width = xglGetTexLevelParameteri(gl,
                                                               GL_TEXTURE_2D,
                                                               level,
                                                               GL_TEXTURE_WIDTH);
            const GLint level_height = xglGetTexLevelParameteri(gl,
                                                                GL_TEXTURE_2D,
                                                                level,
                                                                GL_TEXTURE_HEIGHT);

            assert(level_width == level_height);
            assert(level_width == img_width >> level);

            // writes to GL_PIXEL_PACK_BUFFER
            gl.glGetTexImage(GL_TEXTURE_2D, level, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

            // reads from GL_PIXEL_UNPACK_BUFFER
            gl.glActiveTexture(GL_TEXTURE0);
            gl.glTexSubImage3D(GL_TEXTURE_2D_ARRAY,
                               level,
                               0,
                               0,
                               layer,
                               level_width,
                               level_height,
                               1,
                               GL_RGBA,
                               GL_UNSIGNED_BYTE,
                               nullptr);
        }
    }
}

} // namespace init_array_helper

void OpenGL::initArray(const SharedMMTexture &array, const std::vector<SharedMMTexture> &input)
{
    Legacy::VBO vbo;
    vbo.emplace(getSharedFunctions());

    MMTexture &tex = deref(array);
    QOpenGLTexture &qtex = deref(tex.get());

    std::vector<GLuint> srcNames;
    srcNames.reserve(input.size());
    for (const auto &x : input) {
        srcNames.emplace_back(deref(deref(x).get()).textureId());
    }
    assert(srcNames.size() == input.size());
    assert(qtex.layers() == static_cast<int>(input.size()));

    init_array_helper::buildTexture2dArray(getFunctions(),
                                           vbo.get(),
                                           srcNames,
                                           qtex.textureId(),
                                           qtex.width(),
                                           qtex.height());

    vbo.reset();
}
