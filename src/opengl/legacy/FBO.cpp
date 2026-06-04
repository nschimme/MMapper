// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2023 The MMapper Authors

#include "FBO.h"

#include "../../global/ConfigConsts-Computed.h"
#include "../../global/logging.h"
#include "../OpenGLConfig.h"

#include <QOpenGLContext>
#include <QOpenGLExtraFunctions>

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
    resolvedFormat.setInternalTextureFormat(GL_RGBA8);

    m_resolvedFbo = std::make_unique<QOpenGLFramebufferObject>(physicalSize, resolvedFormat);
    if (!m_resolvedFbo->isValid()) {
        m_resolvedFbo.reset();
        throw std::runtime_error("Failed to create resolved FBO");
    }

    if constexpr (CURRENT_PLATFORM != PlatformEnum::Wasm) {
        // Fix Wayland ghosting by forcing the alpha channel of the blit target to 1.0.
        // We use texture swizzling for better performance than doing it in the shader.
        if (auto *ctx = QOpenGLContext::currentContext()) {
            if (auto *f = ctx->extraFunctions()) {
                f->glBindTexture(GL_TEXTURE_2D, m_resolvedFbo->texture());
                f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A, GL_ONE);
                f->glBindTexture(GL_TEXTURE_2D, 0);
            }
        }
    }

    // Only create the multisampling FBO if requested.
    if (requestedSamples > 0) {
        int actualSamples = std::min(requestedSamples, OpenGLConfig::getMaxSamples());

        if (actualSamples > 0) {
            QOpenGLFramebufferObjectFormat msFormat;
            msFormat.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
            msFormat.setSamples(actualSamples);
            msFormat.setTextureTarget(GL_TEXTURE_2D_MULTISAMPLE);
            msFormat.setInternalTextureFormat(GL_RGBA8);

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

void FBO::resolve()
{
    if (!m_resolvedFbo) {
        return; // Nothing to resolve to
    }

    // If we have a valid multisampling FBO, resolve it to the resolved FBO first.
    if (m_multisamplingFbo && m_multisamplingFbo->isValid()) {
        // NOTE: In WebGL2/GLES 3.0 environments, resolving a multisampled framebuffer
        // requires GL_NEAREST.
        QOpenGLFramebufferObject::blitFramebuffer(m_resolvedFbo.get(),
                                                  m_multisamplingFbo.get(),
                                                  GL_COLOR_BUFFER_BIT,
                                                  GL_NEAREST);
    }
}

GLuint FBO::resolvedTextureId() const
{
    return m_resolvedFbo ? m_resolvedFbo->texture() : 0;
}

} // namespace Legacy
