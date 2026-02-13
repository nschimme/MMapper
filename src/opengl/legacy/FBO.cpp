// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2023 The MMapper Authors

#include "FBO.h"

#include "../../global/logging.h"
#include "../OpenGLConfig.h"

#include <QOpenGLFunctions>

namespace Legacy {

bool LOG_FBO_ALLOCATIONS = true;

void FBO::configure(const Viewport &physicalViewport, int requestedSamples)
{
    // Unconditionally release old FBOs to ensure a clean slate.
    m_multisamplingFbo.reset();
    m_resolvedFbo.reset();

    const QSize physicalSize(physicalViewport.size.x, physicalViewport.size.y);
    if (physicalSize.isEmpty()) {
        if (LOG_FBO_ALLOCATIONS) {
            MMLOG_INFO() << "FBOs destroyed (size empty)";
        }
        return;
    }

    // Always create the resolved FBO. This is our target for MSAA resolve
    // and the primary render target if MSAA is disabled.
    QOpenGLFramebufferObjectFormat resolvedFormat;
    resolvedFormat.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
    resolvedFormat.setSamples(0);
    resolvedFormat.setTextureTarget(GL_TEXTURE_2D);

    m_resolvedFbo = std::make_unique<QOpenGLFramebufferObject>(physicalSize, resolvedFormat);
    if (!m_resolvedFbo->isValid()) {
        m_resolvedFbo.reset();
        throw std::runtime_error("Failed to create resolved FBO");
    }

    // Only create the multisampling FBO if requested.
    if (requestedSamples > 0) {
        int actualSamples = std::min(requestedSamples, OpenGLConfig::getMaxSamples());

        if (actualSamples > 0) {
            QOpenGLFramebufferObjectFormat msFormat;
            msFormat.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
            msFormat.setSamples(actualSamples);
            // NOTE: For WebGL2, multisampled storage MUST be in renderbuffers.
            // QOpenGLFramebufferObject handles this automatically when samples > 0
            // and the target is GL_TEXTURE_2D.
            msFormat.setTextureTarget(GL_TEXTURE_2D);

            m_multisamplingFbo = std::make_unique<QOpenGLFramebufferObject>(physicalSize, msFormat);
            if (!m_multisamplingFbo->isValid()) {
                if (LOG_FBO_ALLOCATIONS) {
                    MMLOG_ERROR()
                        << "Failed to create multisampling FBO. Falling back to no multisampling.";
                }
                m_multisamplingFbo.reset();
            } else if (LOG_FBO_ALLOCATIONS) {
                MMLOG_INFO() << "Created multisampling FBO with " << actualSamples << " samples.";
            }
        }
    }
}

void FBO::bind()
{
    QOpenGLFramebufferObject *fboToBind = m_multisamplingFbo ? m_multisamplingFbo.get()
                                                             : m_resolvedFbo.get();
    if (fboToBind) {
        fboToBind->bind();
    }
}

void FBO::release()
{
    QOpenGLFramebufferObject *fboToRelease = m_multisamplingFbo ? m_multisamplingFbo.get()
                                                                : m_resolvedFbo.get();
    if (fboToRelease) {
        fboToRelease->release();
    }
}

void FBO::blitToDefault()
{
    if (!m_resolvedFbo || !m_resolvedFbo->isValid()) {
        return; // Nothing to blit from
    }

    const QSize size = m_resolvedFbo->size();
    const QRect rect(QPoint(0, 0), size);

    // If we have a valid multisampling FBO, resolve it to the resolved FBO first.
    if (m_multisamplingFbo && m_multisamplingFbo->isValid()) {
        if (m_multisamplingFbo->size() != size) {
            MMLOG_ERROR() << "FBO resolve failed: size mismatch (" << m_multisamplingFbo->width()
                          << "x" << m_multisamplingFbo->height() << " vs " << size.width() << "x"
                          << size.height() << ")";
        } else {
            // NOTE: For multisampled blits, the filter MUST be GL_NEAREST according to
            // WebGL2 and GLES 3.0 specifications. Scaling is also NOT allowed.
            QOpenGLFramebufferObject::blitFramebuffer(m_resolvedFbo.get(),
                                                      rect,
                                                      m_multisamplingFbo.get(),
                                                      rect,
                                                      GL_COLOR_BUFFER_BIT,
                                                      GL_NEAREST);
        }
    }

    // Now blit the (potentially resolved) FBO to the default framebuffer.
    {
        static bool checkedDefaultSamples = false;
        if (!checkedDefaultSamples) {
            GLint samples = 0;
            QOpenGLContext::currentContext()->functions()->glGetIntegerv(GL_SAMPLES, &samples);
            if (samples > 0) {
                MMLOG_WARN() << "Default framebuffer is multisampled (" << samples
                             << " samples). Blitting to it will fail on WebGL2/GLES3.";
            }
            checkedDefaultSamples = true;
        }
    }

    // NOTE: If the default framebuffer (screen) is multisampled (e.g. in some WebGL2
    // environments where "antialias: true" is set on the context), this blit will FAIL
    // with GL_INVALID_OPERATION because GLES 3.0 does not allow blitting INTO a
    // multisampled FBO.
    QOpenGLFramebufferObject::blitFramebuffer(nullptr,
                                              rect,
                                              m_resolvedFbo.get(),
                                              rect,
                                              GL_COLOR_BUFFER_BIT,
                                              GL_LINEAR);
}

} // namespace Legacy
