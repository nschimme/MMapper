// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Nils Schimmelmann <nschimme@gmail.com> (Jahara)

#include "OpenGL.h"

#include "./legacy/Legacy.h"
#include "./legacy/ShaderUtils.h"
#include "OpenGLTypes.h"
#include "../display/MapCanvasConfig.h" // For MapCanvasConfig functions

#include <QOpenGLContext>
#include <QOffscreenSurface>
#include <QSurfaceFormat>
#include <QDebug> // For qInfo/qWarning
#include <vector>
#include <string>
#include <sstream>
#include <cassert>
#include <optional>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#ifdef WIN32
extern "C" {
// Prefer discrete nVidia and AMD GPUs by default on Windows
__declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;
__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}
#endif

namespace { // Anonymous namespace for helper structures and functions

struct GLVersionPair {
    int major;
    int minor;
};

// Helper function to try creating a context and check its validity
static bool checkContext(QSurfaceFormat format) {
    QOpenGLContext context;
    context.setFormat(format);
    if (context.create()) {
        QOffscreenSurface surface;
        surface.setFormat(format);
        surface.create();

        if (!surface.isValid()) {
            qWarning() << "[GL Check] Offscreen surface not valid for format: "
                       << format.majorVersion() << "." << format.minorVersion()
                       << (format.profile() == QSurfaceFormat::CoreProfile ? " Core" : " Compat");
            return false;
        }

        if (!context.makeCurrent(&surface)) {
             qWarning() << "[GL Check] Could not make context current for format: "
                       << format.majorVersion() << "." << format.minorVersion()
                       << (format.profile() == QSurfaceFormat::CoreProfile ? " Core" : " Compat");
            return false;
        }
        bool valid = context.isValid();
        if (valid) {
            // QOpenGLFunctions are specific to a context, so we'd need to get them from the current context
            // For simplicity, we'll assume context.isValid() is enough for this check.
            // If more detailed checking is needed, context.functions()->glGetString can be used.
            qInfo() << "[GL Check] Successfully created context for requested "
                    << format.majorVersion() << "." << format.minorVersion()
                    << (format.profile() == QSurfaceFormat::CoreProfile ? " Core" : " Compat");
        }
        context.doneCurrent();
        return valid;
    }
    qWarning() << "[GL Check] Context creation failed for format: "
               << format.majorVersion() << "." << format.minorVersion()
               << (format.profile() == QSurfaceFormat::CoreProfile ? " Core" : " Compat");
    return false;
}

static std::string determineHighestReportableVersionStringInternal() {
    std::string highestReportedGLVersionString = "Unknown";
    QSurfaceFormat::FormatOptions options = QSurfaceFormat::DebugContext | QSurfaceFormat::DeprecatedFunctions;

    std::vector<GLVersionPair> coreVersions = {
        {4, 6}, {4, 5}, {4, 4}, {4, 3}, {4, 2}, {4, 1}, {4, 0},
        {3, 3}, {3, 2}
    };
    std::vector<GLVersionPair> compatVersions = {
        {3, 2}, {3, 1}, {3, 0}, {2, 1}
    };

    std::string bestCoreVersionStr = "None";
    int bestCoreMajor = 0, bestCoreMinor = 0;

    std::string bestCompatVersionStr = "None";
    int bestCompatMajor = 0, bestCompatMinor = 0;

    QSurfaceFormat testFormat;
    testFormat.setRenderableType(QSurfaceFormat::OpenGL);
    testFormat.setOptions(options);
    testFormat.setDepthBufferSize(24);


    qInfo() << "[GL Probe Internal] Testing Core profiles for reporting...";
    for (const auto& ver : coreVersions) {
        testFormat.setVersion(ver.major, ver.minor);
        testFormat.setProfile(QSurfaceFormat::CoreProfile);
        if (checkContext(testFormat)) {
            std::ostringstream oss;
            oss << "GL " << ver.major << "." << ver.minor << " Core";
            bestCoreVersionStr = oss.str();
            bestCoreMajor = ver.major;
            bestCoreMinor = ver.minor;
            qInfo() << "[GL Probe Internal] Found usable Core for reporting: " << bestCoreVersionStr.c_str();
            break;
        }
    }

    qInfo() << "[GL Probe Internal] Testing Compatibility profiles for reporting...";
    for (const auto& ver : compatVersions) {
        testFormat.setVersion(ver.major, ver.minor);
        testFormat.setProfile(QSurfaceFormat::CompatibilityProfile);
        if (checkContext(testFormat)) {
            std::ostringstream oss;
            oss << "GL " << ver.major << "." << ver.minor << " Compat";
            bestCompatVersionStr = oss.str();
            bestCompatMajor = ver.major;
            bestCompatMinor = ver.minor;
            qInfo() << "[GL Probe Internal] Found usable Compatibility for reporting: " << bestCompatVersionStr.c_str();
            break;
        }
    }

    if (bestCoreMajor > bestCompatMajor || (bestCoreMajor == bestCompatMajor && bestCoreMinor > bestCompatMinor)) {
        highestReportedGLVersionString = bestCoreVersionStr;
    } else if (bestCompatMajor > 0) {
        highestReportedGLVersionString = bestCompatVersionStr;
    } else if (bestCoreMajor > 0) {
        highestReportedGLVersionString = bestCoreVersionStr;
    } else {
        highestReportedGLVersionString = "GL detection failed or no suitable GL context found.";
    }
    qInfo() << "[GL Probe Internal] Highest reportable GL version determined: " << highestReportedGLVersionString.c_str();
    return highestReportedGLVersionString;
}

static QSurfaceFormat determineOptimalRunningFormatInternal() {
    QSurfaceFormat runningFormat;
    runningFormat.setRenderableType(QSurfaceFormat::OpenGL);
    QSurfaceFormat::FormatOptions options = QSurfaceFormat::DebugContext | QSurfaceFormat::DeprecatedFunctions;
    runningFormat.setOptions(options);
    runningFormat.setDepthBufferSize(24);

#ifdef Q_OS_MACOS
    qInfo() << "[GL Setup Internal] macOS detected. Requesting GL 2.1 Compatibility profile for running.";
    runningFormat.setVersion(2, 1);
    runningFormat.setProfile(QSurfaceFormat::CompatibilityProfile);
    if (!checkContext(runningFormat)) {
        qWarning() << "[GL Setup Internal] Critical: macOS GL 2.1 Compatibility profile failed to initialize for running! Falling back to NoProfile.";
        runningFormat.setVersion(0,0);
        runningFormat.setProfile(QSurfaceFormat::NoProfile);
    }
#else
    // For other OS, try to find a working compatibility profile first as a stable option.
    // Fallback to a core profile if no suitable compatibility profile is found.
    std::vector<GLVersionPair> compatVersionsToTry = {{3, 2}, {3, 1}, {3, 0}, {2, 1}};
    std::vector<GLVersionPair> coreVersionsToTry = {{3, 3}, {3, 2}}; // More conservative core versions for running

    bool runningFormatSet = false;
    QSurfaceFormat testFormat;
    testFormat.setRenderableType(QSurfaceFormat::OpenGL);
    testFormat.setOptions(options);
    testFormat.setDepthBufferSize(24);

    qInfo() << "[GL Setup Internal] Non-macOS: Testing Compatibility profiles for running...";
    for (const auto& ver : compatVersionsToTry) {
        testFormat.setVersion(ver.major, ver.minor);
        testFormat.setProfile(QSurfaceFormat::CompatibilityProfile);
        if (checkContext(testFormat)) {
            runningFormat.setVersion(ver.major, ver.minor);
            runningFormat.setProfile(QSurfaceFormat::CompatibilityProfile);
            qInfo() << "[GL Setup Internal] Non-macOS: Setting running format to GL " << ver.major << "." << ver.minor << " Compat";
            runningFormatSet = true;
            break;
        }
    }

    if (!runningFormatSet) {
        qInfo() << "[GL Setup Internal] Non-macOS: No suitable Compatibility profile found. Testing Core profiles for running...";
        for (const auto& ver : coreVersionsToTry) {
            testFormat.setVersion(ver.major, ver.minor);
            testFormat.setProfile(QSurfaceFormat::CoreProfile);
            if (checkContext(testFormat)) {
                runningFormat.setVersion(ver.major, ver.minor);
                runningFormat.setProfile(QSurfaceFormat::CoreProfile);
                qInfo() << "[GL Setup Internal] Non-macOS: Setting running format to GL " << ver.major << "." << ver.minor << " Core";
                runningFormatSet = true;
                break;
            }
        }
    }

    if (!runningFormatSet) {
        qInfo() << "[GL Setup Internal] Non-macOS: No specific GL profile succeeded for running. Using default NoProfile.";
        runningFormat.setVersion(0,0);
        runningFormat.setProfile(QSurfaceFormat::NoProfile);
    }
#endif
    qInfo() << "[GL Setup Internal] Final running format to be used: Version "
            << runningFormat.majorVersion() << "." << runningFormat.minorVersion()
            << " Profile: " << (runningFormat.profile() == QSurfaceFormat::CoreProfile ? "Core" :
                                runningFormat.profile() == QSurfaceFormat::CompatibilityProfile ? "Compat" : "NoProfile");
    return runningFormat;
}

} // end anonymous namespace

void OpenGL::initializeDefaultSurfaceFormat(int antialiasingSamples)
{
    qInfo() << "[GL Setup] Initializing default OpenGL surface format...";

    QSurfaceFormat runningFormat = determineOptimalRunningFormatInternal();
    std::string highestReportableVersion = determineHighestReportableVersionStringInternal();

    // Store the highest reportable version string via MapCanvasConfig
    MapCanvasConfig::setHighestReportableOpenGLVersionString(highestReportableVersion);
    qInfo() << "[GL Setup] Highest reportable GL version stored via MapCanvasConfig: " << highestReportableVersion.c_str();

    // Apply antialiasing samples
    runningFormat.setSamples(antialiasingSamples);
    qInfo() << "[GL Setup] Applied antialiasing samples: " << antialiasingSamples;

    // Set the default format for the entire application
    QSurfaceFormat::setDefaultFormat(runningFormat);
    qInfo() << "[GL Setup] Default QSurfaceFormat set. Running Version: "
            << runningFormat.majorVersion() << "." << runningFormat.minorVersion()
            << " Profile: " << (runningFormat.profile() == QSurfaceFormat::CoreProfile ? "Core" :
                                runningFormat.profile() == QSurfaceFormat::CompatibilityProfile ? "Compat" : "NoProfile")
            << " Samples: " << runningFormat.samples();
}

OpenGL::OpenGL()
    : m_opengl{Legacy::Functions::alloc()}
{}

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

bool OpenGL::tryEnableMultisampling(const int requestedSamples)
{
    return getFunctions().tryEnableMultisampling(requestedSamples);
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
    // screen is [-1,+1]^3.
    const auto fullScreenQuad = std::vector{glm::vec3{-1, -1, 0},
                                            glm::vec3{+1, -1, 0},
                                            glm::vec3{+1, +1, 0},
                                            glm::vec3{-1, +1, 0}};

    const auto oldProj = getProjectionMatrix();
    setProjectionMatrix(glm::mat4(1));
    renderPlainQuads(fullScreenQuad, renderState.withDepthFunction(std::nullopt));
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
