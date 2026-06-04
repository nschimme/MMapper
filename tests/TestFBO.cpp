// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "TestFBO.h"

#include "../src/opengl/legacy/FBO.h"
#include "../src/opengl/OpenGLTypes.h"
#include "../src/opengl/OpenGLConfig.h"

#include <QOpenGLContext>
#include <QOffscreenSurface>
#include <QtTest/QtTest>

TestFBO::TestFBO() = default;
TestFBO::~TestFBO() = default;

void TestFBO::testConfigure()
{
    QSurfaceFormat format;
    format.setRenderableType(QSurfaceFormat::OpenGL);
    format.setVersion(3, 3);
    format.setProfile(QSurfaceFormat::CoreProfile);

    QOpenGLContext context;
    context.setFormat(format);
    if (!context.create()) {
        format.setRenderableType(QSurfaceFormat::OpenGLES);
        format.setVersion(3, 0);
        context.setFormat(format);
        if (!context.create()) {
            QSKIP("OpenGL/GLES context creation failed, skipping test");
        }
    }

    QOffscreenSurface surface;
    surface.setFormat(context.format());
    surface.create();
    if (!surface.isValid()) {
        QSKIP("Offscreen surface creation failed, skipping test");
    }

    if (!context.makeCurrent(&surface)) {
        QSKIP("makeCurrent failed, skipping test");
    }

    // Initialize OpenGLConfig to avoid issues with zeroed maxSamples
    OpenGLConfig::setMaxSamples(4);

    Legacy::FBO fbo;
    Viewport viewport{{0, 0}, {100, 100}};

    // Test basic configuration
    try {
        fbo.configure(viewport, 0);
    } catch (const std::exception &e) {
        QFAIL(qPrintable(QString("FBO::configure failed: %1").arg(e.what())));
    }

    QVERIFY(fbo.resolvedTextureId() != 0);

    context.doneCurrent();
}

QTEST_MAIN(TestFBO)
