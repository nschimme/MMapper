// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Nils Schimmelmann <nschimme@gmail.com> (Jahara)

#include "OpenGL.h"

#include "./legacy/Legacy.h"
#include "./legacy/ShaderUtils.h"
#include "OpenGLTypes.h"
#include "OpenGLInfo.h"                // For OpenGLInfo functions
#include "../global/logging.h"         // For MMLOG macros

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

#include <utility> // For std::pair

namespace { // Anonymous namespace for helper structures and functions

using GLVersionPair = std::pair<int, int>;

// Helper function to try creating a context and check its validity using glGetIntegerv
static bool checkContext(QSurfaceFormat formatToRequest, int requestedMajor, int requestedMinor, QSurfaceFormat::OpenGLContextProfile requestedProfile) {
    QOpenGLContext context;
    context.setFormat(formatToRequest);
    if (!context.create()) {
        MMLOG_DEBUG() << "[GL Check] context.create() failed for requested "
                      << requestedMajor << "." << requestedMinor
                      << (requestedProfile == QSurfaceFormat::CoreProfile ? " Core" : " Compat")
                      << " with options: Debug(" << formatToRequest.testOption(QSurfaceFormat::DebugContext)
                      << "), Deprecated(" << formatToRequest.testOption(QSurfaceFormat::DeprecatedFunctions) << ")";
        return false;
    }

    QOffscreenSurface surface;
    surface.setFormat(formatToRequest);
    surface.create();

    if (!surface.isValid()) {
        MMLOG_WARNING() << "[GL Check] Offscreen surface not valid for requested "
                        << requestedMajor << "." << requestedMinor
                        << (requestedProfile == QSurfaceFormat::CoreProfile ? " Core" : " Compat");
        return false;
    }

    if (!context.makeCurrent(&surface)) {
         MMLOG_WARNING() << "[GL Check] Could not make context current for requested "
                         << requestedMajor << "." << requestedMinor
                         << (requestedProfile == QSurfaceFormat::CoreProfile ? " Core" : " Compat");
        return false;
    }

    if (!context.isValid()) {
        MMLOG_WARNING() << "[GL Check] Context became invalid after makeCurrent for requested "
                         << requestedMajor << "." << requestedMinor
                         << (requestedProfile == QSurfaceFormat::CoreProfile ? " Core" : " Compat");
        context.doneCurrent();
        return false;
    }

    QOpenGLFunctions *glFuncs = context.functions();
    if (!glFuncs) {
        MMLOG_WARNING() << "[GL Check] Failed to retrieve QOpenGLFunctions.";
        context.doneCurrent();
        return false;
    }

    GLint actualMajor = 0, actualMinor = 0, actualProfileMask = 0, actualFlags = 0;
    glFuncs->glGetIntegerv(GL_MAJOR_VERSION, &actualMajor);
    glFuncs->glGetIntegerv(GL_MINOR_VERSION, &actualMinor);

    bool isActualCore = false;
    bool isActualCompat = false;

    if (actualMajor > 3 || (actualMajor == 3 && actualMinor >= 2)) {
        glFuncs->glGetIntegerv(GL_CONTEXT_PROFILE_MASK, &actualProfileMask);
        if (actualProfileMask & GL_CONTEXT_CORE_PROFILE_BIT) {
            isActualCore = true;
        }
        if (actualProfileMask & GL_CONTEXT_COMPATIBILITY_PROFILE_BIT) {
            isActualCompat = true;
        }
    } else {
        isActualCompat = true; // Assume compatibility for older versions
    }

    bool actualHasDebugFlag = false;
    // Check GL_CONTEXT_FLAG_DEBUG_BIT if GL version is 4.3+
    if (actualMajor > 4 || (actualMajor == 4 && actualMinor >= 3)) {
         glFuncs->glGetIntegerv(GL_CONTEXT_FLAGS, &actualFlags);
         if (actualFlags & GL_CONTEXT_FLAG_DEBUG_BIT) {
             actualHasDebugFlag = true;
         }
    }
    // For older versions, if DebugContext was requested via QSurfaceFormat and context creation succeeded,
    // we assume Qt/driver handled it. This is a best-effort guess if the specific GL_CONTEXT_FLAG_DEBUG_BIT is not available.
    else if (formatToRequest.testOption(QSurfaceFormat::DebugContext)) {
        actualHasDebugFlag = true;
    }


    bool versionOk = (actualMajor > requestedMajor) ||
                     (actualMajor == requestedMajor && actualMinor >= requestedMinor);

    bool profileOk = false;
    if (requestedProfile == QSurfaceFormat::NoProfile) { // If no specific profile was requested, any is fine
        profileOk = true;
    } else if (requestedMajor < 3 || (requestedMajor == 3 && requestedMinor < 2)) { // For GL < 3.2
        if (requestedProfile == QSurfaceFormat::CoreProfile) {
            profileOk = false; // Core profile did not exist before GL 3.2
        } else { // Compatibility or NoProfile requested
            profileOk = isActualCompat; // Must be compatibility-like
        }
    } else { // For GL 3.2+
        if (requestedProfile == QSurfaceFormat::CoreProfile) {
            profileOk = isActualCore;
        } else if (requestedProfile == QSurfaceFormat::CompatibilityProfile) {
            profileOk = isActualCompat;
        }
    }

    // If a debug context was explicitly requested via formatToRequest, then actualHasDebugFlag must be true.
    bool debugOk = !formatToRequest.testOption(QSurfaceFormat::DebugContext) || actualHasDebugFlag;

    if (versionOk && profileOk && debugOk) {
        MMLOG_DEBUG() << "[GL Check Direct] Successfully created AND validated context. Requested: "
                      << requestedMajor << "." << requestedMinor
                      << (requestedProfile == QSurfaceFormat::CoreProfile ? " Core" : (requestedProfile == QSurfaceFormat::CompatibilityProfile ? " Compat" : " NoProfile"))
                      << " (ReqDebug: " << formatToRequest.testOption(QSurfaceFormat::DebugContext) << ")"
                      << ". Got: " << actualMajor << "." << actualMinor
                      << (isActualCore ? " Core" : (isActualCompat ? " Compat" : " UnknownProfile"))
                      << " (ActualDebug: " << actualHasDebugFlag << ")";
        context.doneCurrent();
        return true;
    } else {
        MMLOG_DEBUG() << "[GL Check Direct] Context created, but mismatch or condition not met. Requested: "
                      << requestedMajor << "." << requestedMinor
                      << (requestedProfile == QSurfaceFormat::CoreProfile ? " Core" : (requestedProfile == QSurfaceFormat::CompatibilityProfile ? " Compat" : " NoProfile"))
                      << " (ReqDebug: " << formatToRequest.testOption(QSurfaceFormat::DebugContext) << ")"
                      << ". Got: " << actualMajor << "." << actualMinor
                      << (isActualCore ? " Core" : (isActualCompat ? " Compat" : " UnknownProfile"))
                      << " (ActualDebug: " << actualHasDebugFlag << ")"
                      << ". VersionOK: " << versionOk << " ProfileOK: " << profileOk << " DebugOK: " << debugOk;
        context.doneCurrent();
        return false;
    }
}

static std::string determineHighestReportableVersionStringInternal() {
    std::string highestReportedGLVersionString = "Unknown";
    int highestMajor = 0;
    int highestMinor = 0;
    bool highestIsCore = false;
    bool highestHasDebug = false;

    QSurfaceFormat::FormatOptions optionsWithDebug = QSurfaceFormat::DebugContext | QSurfaceFormat::DeprecatedFunctions;
    QSurfaceFormat::FormatOptions optionsCompatOnly = QSurfaceFormat::DeprecatedFunctions;
    QSurfaceFormat::FormatOptions optionsCoreOnly = QSurfaceFormat::FormatOption(0); // No specific options for core

    // Single list of versions to test, from highest to lowest.
    std::vector<GLVersionPair> versionsToTestReport = {
        {4, 6}, {4, 5}, {4, 4}, {4, 3}, {4, 2}, {4, 1}, {4, 0},
        {3, 3}, {3, 2}, {3, 1}, {3, 0},
        {2, 1}
    };

    QSurfaceFormat testFormat;
    testFormat.setRenderableType(QSurfaceFormat::OpenGL);
    testFormat.setDepthBufferSize(24);

    auto updateHighest = [&](int major, int minor, bool isCore, bool hasDebug) {
        if (major > highestMajor || (major == highestMajor && minor > highestMinor)) {
            highestMajor = major;
            highestMinor = minor;
            highestIsCore = isCore;
            highestHasDebug = hasDebug;
        } else if (major == highestMajor && minor == highestMinor) {
            // Prefer Compatibility over Core if versions are identical
            if (!isCore && highestIsCore) { // New is Compat, Old was Core
                highestIsCore = false; // Switch to Compat
                highestHasDebug = hasDebug; // Update debug status with the new profile's debug status
            } else if (isCore == highestIsCore) { // Profiles are the same
                // Prefer with debug if versions and profiles are identical
                if (hasDebug && !highestHasDebug) {
                    highestHasDebug = true;
                }
            }
        }
    };

    MMLOG_INFO() << "[GL Probe Internal] Determining highest reportable GL version...";
    for (const auto& ver : versionsToTestReport) {
        // Test Core Profile for this version
        testFormat.setVersion(ver.first, ver.second);
        testFormat.setProfile(QSurfaceFormat::CoreProfile);

        MMLOG_DEBUG() << "[GL Probe Internal] Testing GL " << ver.first << "." << ver.second << " Core (Debug)...";
        testFormat.setOptions(optionsWithDebug);
        if (checkContext(testFormat, ver.first, ver.second, QSurfaceFormat::CoreProfile)) {
            updateHighest(ver.first, ver.second, true, true);
            // MMLOG_DEBUG() already in checkContext for success
        } else {
            MMLOG_DEBUG() << "[GL Probe Internal] Testing GL " << ver.first << "." << ver.second << " Core...";
            testFormat.setOptions(optionsCoreOnly);
            if (checkContext(testFormat, ver.first, ver.second, QSurfaceFormat::CoreProfile)) {
                updateHighest(ver.first, ver.second, true, false);
            }
        }

        // Test Compatibility Profile for this version
        // Version remains the same (ver.first, ver.second)
        testFormat.setProfile(QSurfaceFormat::CompatibilityProfile);

        MMLOG_DEBUG() << "[GL Probe Internal] Testing GL " << ver.first << "." << ver.second << " Compat (Debug)...";
        testFormat.setOptions(optionsWithDebug);
        if (checkContext(testFormat, ver.first, ver.second, QSurfaceFormat::CompatibilityProfile)) {
            updateHighest(ver.first, ver.second, false, true);
        } else {
            MMLOG_DEBUG() << "[GL Probe Internal] Testing GL " << ver.first << "." << ver.second << " Compat...";
            testFormat.setOptions(optionsCompatOnly);
            if (checkContext(testFormat, ver.first, ver.second, QSurfaceFormat::CompatibilityProfile)) {
                updateHighest(ver.first, ver.second, false, false);
            }
        }
    }

    if (highestMajor > 0) {
        std::ostringstream oss;
        oss << "GL " << highestMajor << "." << highestMinor
            << (highestIsCore ? " Core" : " Compat")
            << (highestHasDebug ? " (Debug)" : "");
        highestReportedGLVersionString = oss.str();
    } else {
        // If no context was found at all, default to indicating GL 2.1 Compat as a baseline/fallback expectation.
        highestReportedGLVersionString = "GL 2.1 Compat (Probing Failed - Default)";
        MMLOG_ERROR() << "[GL Probe Internal] No suitable GL context found through probing. Reporting fallback: " << highestReportedGLVersionString.c_str();
    }

    MMLOG_INFO() << "[GL Probe Internal] Highest reportable GL version determined: " << highestReportedGLVersionString.c_str();
    return highestReportedGLVersionString;
}

static QSurfaceFormat determineOptimalRunningFormatInternal() {
    QSurfaceFormat runningFormat; // This will store the chosen format including options
    runningFormat.setRenderableType(QSurfaceFormat::OpenGL);
    runningFormat.setDepthBufferSize(24);

    QSurfaceFormat::FormatOptions optionsWithDebug = QSurfaceFormat::DebugContext | QSurfaceFormat::DeprecatedFunctions;
    QSurfaceFormat::FormatOptions optionsCompatOnly = QSurfaceFormat::DeprecatedFunctions;
    QSurfaceFormat::FormatOptions optionsCoreOnly = QSurfaceFormat::FormatOption(0);

    // Define a general-purpose list of versions to try for the running context.
    // Prioritize compatibility, then reasonable core versions.
    // GL 2.1 Compat is essential for macOS and a good general fallback.
    std::vector<std::tuple<GLVersionPair, QSurfaceFormat::OpenGLContextProfile, QSurfaceFormat::FormatOptions, std::string>> profilesToTry;

    // Compatibility profiles to try for the RUNNING context.
    // Prioritize these. Include higher versions.
    std::vector<GLVersionPair> compatVersionsRun = {
        {4, 6}, {4, 5}, {4, 4}, {4, 3}, {4, 2}, {4, 1}, {4, 0}, // Try higher compats
        {3, 3}, {3, 2}, {3, 1}, {3, 0},
        {2, 1}  // Essential fallback
    };
    for (const auto& ver : compatVersionsRun) {
        profilesToTry.emplace_back(ver, QSurfaceFormat::CompatibilityProfile, optionsWithDebug, "Compat (Debug)");
        profilesToTry.emplace_back(ver, QSurfaceFormat::CompatibilityProfile, optionsCompatOnly, "Compat");
    }

    // Core profiles to try for the RUNNING context (as a fallback if no suitable Compat found).
    // Keep this list relatively conservative for stability, e.g., 3.2+
    std::vector<GLVersionPair> coreVersionsRun = {
        {4, 6}, {4, 5}, {4, 4}, {4, 3}, {4, 2}, {4, 1}, {4, 0},
        {3, 3}, {3, 2}
    };
    for (const auto& ver : coreVersionsRun) {
        profilesToTry.emplace_back(ver, QSurfaceFormat::CoreProfile, optionsWithDebug, "Core (Debug)");
        profilesToTry.emplace_back(ver, QSurfaceFormat::CoreProfile, optionsCoreOnly, "Core");
    }

    QSurfaceFormat testFormat;
    testFormat.setRenderableType(QSurfaceFormat::OpenGL);
    testFormat.setDepthBufferSize(24);

    MMLOG_INFO() << "[GL Setup Internal] Determining optimal running GL format...";
    for (const auto& trial : profilesToTry) {
        const GLVersionPair& ver = std::get<0>(trial); // GLVersionPair is now std::pair<int, int>
        QSurfaceFormat::OpenGLContextProfile profile = std::get<1>(trial);
        QSurfaceFormat::FormatOptions currentOptions = std::get<2>(trial);
        const std::string& profileDesc = std::get<3>(trial);

        testFormat.setVersion(ver.first, ver.second);
        testFormat.setProfile(profile);
        testFormat.setOptions(currentOptions);

        MMLOG_DEBUG() << "[GL Setup Internal] Trying: GL " << ver.first << "." << ver.second << " " << profileDesc.c_str();
        if (checkContext(testFormat, ver.first, ver.second, profile)) {
            runningFormat.setVersion(ver.first, ver.second);
            runningFormat.setProfile(profile);
            runningFormat.setOptions(currentOptions); // Store the options that worked
            MMLOG_INFO() << "[GL Setup Internal] Successfully set running format to: GL "
                         << ver.first << "." << ver.second << " " << profileDesc.c_str();
            // MMLOG_INFO for final running format is now just before return / at the end of function
            return runningFormat;
        }
    }

    // Fallback if nothing specific worked from the main list
    MMLOG_WARNING() << "[GL Setup Internal] No specific GL profile from the main list succeeded. Attempting explicit GL 2.1 Compat fallback.";

    // testFormat is reused
    const int fallbackMajor = 2, fallbackMinor = 1;
    const QSurfaceFormat::OpenGLContextProfile fallbackProfile = QSurfaceFormat::CompatibilityProfile;

    // Try GL 2.1 Compat with Debug
    testFormat.setVersion(fallbackMajor, fallbackMinor);
    testFormat.setProfile(fallbackProfile);
    testFormat.setOptions(optionsWithDebug);
    MMLOG_DEBUG() << "[GL Setup Internal] Trying fallback: GL " << fallbackMajor << "." << fallbackMinor << " Compat (Debug)";
    if (checkContext(testFormat, fallbackMajor, fallbackMinor, fallbackProfile)) {
        runningFormat.setVersion(fallbackMajor, fallbackMinor);
        runningFormat.setProfile(fallbackProfile);
        runningFormat.setOptions(optionsWithDebug);
        MMLOG_INFO() << "[GL Setup Internal] Using GL 2.1 Compat (Debug) as fallback running format.";
    } else {
        // Try GL 2.1 Compat without Debug
        MMLOG_DEBUG() << "[GL Setup Internal] Trying fallback: GL " << fallbackMajor << "." << fallbackMinor << " Compat";
        testFormat.setOptions(optionsCompatOnly);
        if (checkContext(testFormat, fallbackMajor, fallbackMinor, fallbackProfile)) {
            runningFormat.setVersion(fallbackMajor, fallbackMinor);
            runningFormat.setProfile(fallbackProfile);
            runningFormat.setOptions(optionsCompatOnly);
            MMLOG_INFO() << "[GL Setup Internal] Using GL 2.1 Compat as fallback running format.";
        } else {
            MMLOG_ERROR() << "[GL Setup Internal] Fallback to GL 2.1 Compat also failed! Using NoProfile.";
            runningFormat.setVersion(0,0);
            runningFormat.setProfile(QSurfaceFormat::NoProfile);
            runningFormat.setOptions(QSurfaceFormat::FormatOption(0)); // Clear options too
        }
    }

    MMLOG_INFO() << "[GL Setup Internal] Final running format to be used: Version "
                 << runningFormat.majorVersion() << "." << runningFormat.minorVersion()
                         << " Profile: " << (runningFormat.profile() == QSurfaceFormat::CoreProfile ? "Core" :
                                             runningFormat.profile() == QSurfaceFormat::CompatibilityProfile ? "Compat" : "NoProfile")
                         << " Options: Debug(" << (runningFormat.options().testFlag(QSurfaceFormat::DebugContext) ? "Yes" : "No")
                         << "), Deprecated(" << (runningFormat.options().testFlag(QSurfaceFormat::DeprecatedFunctions) ? "Yes" : "No") << ")";
            return runningFormat;
        }
    }

    // Fallback if nothing specific worked from the main list
    MMLOG_WARNING() << "[GL Setup Internal] No specific GL profile from the main list succeeded. Attempting explicit GL 2.1 Compat fallback.";

    testFormat.setVersion(2, 1);
    testFormat.setProfile(QSurfaceFormat::CompatibilityProfile);

    // Try GL 2.1 Compat with Debug
    testFormat.setOptions(optionsWithDebug);
    MMLOG_DEBUG() << "[GL Setup Internal] Trying fallback: GL 2.1 Compat (Debug)";
    if (checkContext(testFormat)) {
        runningFormat.setVersion(2, 1);
        runningFormat.setProfile(QSurfaceFormat::CompatibilityProfile);
        runningFormat.setOptions(optionsWithDebug);
        MMLOG_INFO() << "[GL Setup Internal] Using GL 2.1 Compat (Debug) as fallback running format.";
    } else {
        // Try GL 2.1 Compat without Debug
        MMLOG_DEBUG() << "[GL Setup Internal] Trying fallback: GL 2.1 Compat";
        testFormat.setOptions(optionsCompatOnly);
        if (checkContext(testFormat)) {
            runningFormat.setVersion(2, 1);
            runningFormat.setProfile(QSurfaceFormat::CompatibilityProfile);
            runningFormat.setOptions(optionsCompatOnly);
            MMLOG_INFO() << "[GL Setup Internal] Using GL 2.1 Compat as fallback running format.";
        } else {
            MMLOG_ERROR() << "[GL Setup Internal] Fallback to GL 2.1 Compat also failed! Using NoProfile.";
            runningFormat.setVersion(0,0);
            runningFormat.setProfile(QSurfaceFormat::NoProfile);
            runningFormat.setOptions(QSurfaceFormat::FormatOption(0)); // Clear options too
        }
    }

    MMLOG_INFO() << "[GL Setup Internal] Final running format to be used: Version "
                 << runningFormat.majorVersion() << "." << runningFormat.minorVersion()
                 << " Profile: " << (runningFormat.profile() == QSurfaceFormat::CoreProfile ? "Core" :
                                     runningFormat.profile() == QSurfaceFormat::CompatibilityProfile ? "Compat" : "NoProfile")
                 << " Options: Debug(" << (runningFormat.options().testFlag(QSurfaceFormat::DebugContext) ? "Yes" : "No")
                 << "), Deprecated(" << (runningFormat.options().testFlag(QSurfaceFormat::DeprecatedFunctions) ? "Yes" : "No") << ")";
    return runningFormat;
}

} // end anonymous namespace

void OpenGL::initializeDefaultSurfaceFormat(int antialiasingSamples)
{
    MMLOG_INFO() << "[GL Setup] Initializing default OpenGL surface format...";

    QSurfaceFormat runningFormat = determineOptimalRunningFormatInternal();
    std::string highestReportableVersion = determineHighestReportableVersionStringInternal();

    // Store the highest reportable version string via OpenGLInfo
    OpenGLInfo::setHighestReportableVersionString(highestReportableVersion);
    // The MMLOG_INFO for this is now inside OpenGLInfo::setHighestReportableVersionString

    // Apply antialiasing samples
    runningFormat.setSamples(antialiasingSamples);
    MMLOG_INFO() << "[GL Setup] Applied antialiasing samples: " << antialiasingSamples;

    // Set the default format for the entire application
    QSurfaceFormat::setDefaultFormat(runningFormat);
    MMLOG_INFO() << "[GL Setup] Default QSurfaceFormat set. Running Version: "
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
