// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2023 The MMapper Authors

#include "FBO.h"

#include "../global/logging.h"
#include "OpenGLConfig.h"

#include <QOpenGLFramebufferObject>
#include <QOpenGLFramebufferObjectFormat>

FBO::FBO() = default;
FBO::~FBO() = default;

void FBO::configure(const QSize &size, int requestedSamples, float devicePixelRatio)
{
    // Unconditionally release old FBOs to ensure a clean slate.
    m_multisamplingFbo.reset();
    m_resolvedFbo.reset();

    const QSize physicalSize(size.width() * static_cast<int>(devicePixelRatio),
                             size.height() * static_cast<int>(devicePixelRatio));

    if (physicalSize.isEmpty()) {
        MMLOG_INFO() << "FBOs destroyed (size empty)";
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
        MMLOG_ERROR() << "Failed to create resolved FBO. Rendering will be broken.";
        m_resolvedFbo.reset();
        return; // Can't proceed
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
                MMLOG_ERROR()
                    << "Failed to create multisampling FBO. Falling back to no multisampling.";
                m_multisamplingFbo.reset();
            } else {
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
    if (!m_resolvedFbo) {
        return; // Nothing to blit from
    }

    // If we have a valid multisampling FBO, resolve it to the resolved FBO first.
    if (m_multisamplingFbo && m_multisamplingFbo->isValid()) {
        QOpenGLFramebufferObject::blitFramebuffer(m_resolvedFbo.get(),
                                                  m_multisamplingFbo.get(),
                                                  GL_COLOR_BUFFER_BIT,
                                                  GL_LINEAR);
    }

    // Now blit the (potentially resolved) FBO to the default framebuffer.
    QOpenGLFramebufferObject::blitFramebuffer(nullptr,
                                              m_resolvedFbo.get(),
                                              GL_COLOR_BUFFER_BIT,
                                              GL_NEAREST);
}
