// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Nils Schimmelmann <nschimme@gmail.com> (Jahara)

#include "OpenGL.h"

#include "../global/ConfigConsts.h"
#include "./legacy/FunctionsES30.h"
#include "./legacy/FunctionsGL33.h"
#include "./legacy/Legacy.h"
#include "./legacy/Meshes.h"
#include "OpenGLConfig.h"
#include "OpenGLProber.h"
#include "OpenGLTypes.h"

#include <cassert>
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

void OpenGL::setMultisamplingFbo(int requestedSamples, const QSize &size)
{
    // Calculate physical size based on logical size and device pixel ratio
    const float dpr = getDevicePixelRatio();
    const QSize physicalSize(size.width() * static_cast<int>(dpr),
                             size.height() * static_cast<int>(dpr));

    // Always manage the resolved FBO if size is not empty
    if (!physicalSize.isEmpty()) {
        QOpenGLFramebufferObjectFormat resolvedFormat;
        resolvedFormat.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
        resolvedFormat.setSamples(0); // Not multisampled
        resolvedFormat.setTextureTarget(GL_TEXTURE_2D);
        resolvedFormat.setInternalTextureFormat(GL_RGBA8);

        if (!m_resolvedFbo || m_resolvedFbo->size() != physicalSize) {
            m_resolvedFbo = std::make_unique<QOpenGLFramebufferObject>(physicalSize, resolvedFormat);
            if (!m_resolvedFbo->isValid()) {
                MMLOG_ERROR() << "Failed to create resolved FBO with physical size "
                              << physicalSize.width() << "x" << physicalSize.height()
                              << ". This may cause rendering issues.";
                m_resolvedFbo.reset(); // Indicate failure
            } else {
                MMLOG_INFO() << "Created resolved FBO with physical size "
                             << m_resolvedFbo->size().width() << "x"
                             << m_resolvedFbo->size().height();
            }
        }
    } else {
        m_resolvedFbo.reset();
        MMLOG_INFO() << "Resolved FBO destroyed (size empty)";
    }

    // Manage the multisampling FBO only if samples > 0 and size is not empty
    if (requestedSamples > 0 && !physicalSize.isEmpty()) {
        auto &gl = getFunctions();
        GLint maxSamples = 0;
        gl.glGetIntegerv(GL_MAX_SAMPLES, &maxSamples);
        int actualSamples = std::min(requestedSamples, static_cast<int>(maxSamples));

        if (actualSamples == 0 && requestedSamples > 0) {
            MMLOG_WARNING() << "Requested " << requestedSamples
                            << " samples, but max supported is 0. Disabling multisampling.";
        } else if (actualSamples < requestedSamples) {
            MMLOG_INFO() << "Requested " << requestedSamples << " samples, but using "
                         << actualSamples << " (max supported).";
        }

        QOpenGLFramebufferObjectFormat msFormat;
        msFormat.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
        msFormat.setSamples(actualSamples);
        msFormat.setTextureTarget(GL_TEXTURE_2D_MULTISAMPLE);
        msFormat.setInternalTextureFormat(GL_RGBA8);

        if (!m_multisamplingFbo || m_multisamplingFbo->size() != physicalSize
            || m_multisamplingFbo->format().samples() != actualSamples) {
            m_multisamplingFbo = std::make_unique<QOpenGLFramebufferObject>(physicalSize, msFormat);
            if (!m_multisamplingFbo->isValid()) {
                MMLOG_ERROR() << "Failed to create multisampling FBO with " << actualSamples
                              << " samples and physical size " << physicalSize.width() << "x"
                              << physicalSize.height() << ". Falling back to no multisampling.";
                m_multisamplingFbo.reset(); // Fallback
            } else {
                MMLOG_INFO() << "Created multisampling FBO with "
                             << m_multisamplingFbo->format().samples()
                             << " samples and physical size " << m_multisamplingFbo->size().width()
                             << "x" << m_multisamplingFbo->size().height();
            }
        }
    } else {
        m_multisamplingFbo.reset();
        MMLOG_INFO() << "Multisampling FBO destroyed (samples <= 0 or size empty)";
    }
}

QOpenGLFramebufferObject *OpenGL::getRenderFbo() const
{
    // Render to the multisampling FBO if it's valid, otherwise render to the resolved FBO
    if (m_multisamplingFbo && m_multisamplingFbo->isValid()) {
        return m_multisamplingFbo.get();
    }
    // Fallback to resolved FBO (which should always be valid if size is not empty)
    return m_resolvedFbo.get();
}

void OpenGL::bindMultisamplingFbo()
{
    if (m_multisamplingFbo) {
        m_multisamplingFbo->bind();
    }
}

void OpenGL::releaseMultisamplingFbo()
{
    if (m_multisamplingFbo) {
        m_multisamplingFbo->release();
    }
}

void OpenGL::blitResolvedToDefault(const QSize & /*size*/)
{
    if (!m_resolvedFbo || !m_resolvedFbo->isValid()) {
        MMLOG_WARNING() << "Resolved FBO not valid for blitting. Skipping blit sequence.";
        return;
    }

    // If multisampling FBO is valid, blit from it to the resolved FBO first
    if (m_multisamplingFbo && m_multisamplingFbo->isValid()) {
        QOpenGLFramebufferObject::blitFramebuffer(m_resolvedFbo.get(),
                                                  m_multisamplingFbo.get(),
                                                  GL_COLOR_BUFFER_BIT,
                                                  GL_LINEAR); // Use GL_LINEAR for filtering during resolve
    }
    // Always blit from the resolved FBO to the default framebuffer (displays on screen)
    QOpenGLFramebufferObject::blitFramebuffer(
        nullptr, // Default framebuffer
        m_resolvedFbo.get(),
        GL_COLOR_BUFFER_BIT,
        GL_NEAREST); // GL_NEAREST is usually fine for 1:1 blit to screen
}

bool OpenGL::tryEnableMultisampling(const int requestedSamples)
{
    // This function now primarily manages the GL_MULTISAMPLE state for the default framebuffer.
    // The FBO handles the actual multisampling rendering.
    auto &gl = getFunctions();
    if (requestedSamples > 0) {
        gl.glEnable(GL_MULTISAMPLE);
        // The old smoothing hints might still be useful as a fallback or in conjunction,
        // but the primary multisampling is now via FBO. We can keep them for now.
        gl.glEnable(GL_LINE_SMOOTH);
        gl.glDisable(GL_POLYGON_SMOOTH);
        gl.glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
        return true;
    } else {
        gl.glDisable(GL_MULTISAMPLE);
        gl.glDisable(GL_LINE_SMOOTH);
        gl.glDisable(GL_POLYGON_SMOOTH);
        return false;
    }
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
