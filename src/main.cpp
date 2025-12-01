// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Ulf Hermann <ulfonk_mennhar@gmx.de> (Alve)
// Author: Marek Krejza <krejza@gmail.com> (Caligor)
// Author: Nils Schimmelmann <nschimme@gmail.com> (Jahara)

#include "./configuration/configuration.h"
#include "./display/Filenames.h"
#include "./global/ConfigConsts.h"
#include "./global/WinSock.h"
#include "./global/emojis.h"
#include "./mainwindow/WinDarkMode.h"
#include "./mainwindow/mainwindow.h"
#include "./opengl/OpenGL.h"
#include "./opengl/core/Backend.h"

#include <QOpenGLContext>
#include <QSurfaceFormat>

#include <memory>
#include <optional>

#include <QMessageBox>
#include <QPixmap>
#include <QtCore>
#include <QtWidgets>

#ifdef WITH_DRMINGW
#include <exchndl.h>
#endif

static Backend g_backend;

namespace { // anonymous

struct GLVersion
{
    int major = 0;
    int minor = 0;

    NODISCARD bool operator>(const GLVersion &other) const
    {
        return (major > other.major) || (major == other.major && minor > other.minor);
    }

    NODISCARD bool operator<(const GLVersion &other) const
    {
        return (major < other.major) || (major == other.major && minor < other.minor);
    }
};

inline std::ostream &operator<<(std::ostream &os, const GLVersion &version)
{
    os << version.major << "." << version.minor;
    return os;
}

struct GLContextCheckResult
{
    bool valid = false;
    GLVersion version = {0, 0};
    bool isCore = false;
    bool isCompat = false;
    bool isDeprecated = false;
    bool isDebug = false;
};

NODISCARD GLContextCheckResult checkContext(QSurfaceFormat format,
                                            GLVersion version,
                                            QSurfaceFormat::OpenGLContextProfile profile)
{
    GLContextCheckResult result;
    QOpenGLContext context;
    context.setFormat(format);
    if (!context.create()) {
        MMLOG_DEBUG() << "[GL Check] context.create() failed for requested " << version
                      << (profile == QSurfaceFormat::CoreProfile ? " Core" : " Compat");
        return result;
    }

    QSurfaceFormat actualFormat = context.format();

    result.isCore = (actualFormat.profile() & QSurfaceFormat::CoreProfile);
    result.isCompat = (actualFormat.profile() & QSurfaceFormat::CompatibilityProfile);
    result.isDeprecated = (actualFormat.options() & QSurfaceFormat::DeprecatedFunctions);
    result.isDebug = (actualFormat.options() & QSurfaceFormat::DebugContext);
    result.version = GLVersion{actualFormat.majorVersion(), actualFormat.minorVersion()};

    context.doneCurrent();

    // Check if the actual OpenGL context meets the minimum requirements
    bool profileOk = false;

    if (version < GLVersion{3, 2}) {
        if (profile == QSurfaceFormat::CoreProfile) {
            // Core profile did not exist before GL 3.2
            profileOk = false;
        } else {
            // Must be compatibility-like
            profileOk = result.isCompat || result.isDeprecated;
        }
    } else {
        // For GL 3.2+
        if (profile == QSurfaceFormat::CoreProfile) {
            profileOk = result.isCore;
        } else if (profile == QSurfaceFormat::CompatibilityProfile) {
            profileOk = result.isCompat && result.isDeprecated;
        }
    }

    // If the profile is ok, we consider the context valid even if the version is lower than requested.
    if (profileOk) {
        result.valid = true;
        MMLOG_DEBUG() << "[GL Probe] GL " << result.version << (result.isCore ? " Core" : " Compat")
                      << " is valid";
    }
    return result;
}

struct OpenGLProbeResult
{
    QSurfaceFormat runningFormat = QSurfaceFormat::defaultFormat();
    std::string highestVersion = "Fallback";
    Backend backend;
    bool valid = false;
};

NODISCARD std::string formatGLVersionString(const GLContextCheckResult &result)
{
    std::ostringstream oss;
    oss << "GL" << result.version;
    if (!result.isDeprecated && result.version > GLVersion{3, 1}) {
        oss << "core";
    }
    return oss.str();
}

NODISCARD std::optional<GLContextCheckResult> probeCore(QSurfaceFormat &testFormat,
                                                        std::vector<GLVersion> &coreVersions,
                                                        QSurfaceFormat::FormatOptions optionsCoreOnly)
{
    std::optional<GLContextCheckResult> coreResult = std::nullopt;
    for (auto it = coreVersions.begin(); it != coreVersions.end();) {
        const auto &version = *it;
        testFormat.setVersion(version.major, version.minor);
        testFormat.setProfile(QSurfaceFormat::CoreProfile);
        testFormat.setOptions(optionsCoreOnly);

        GLContextCheckResult contextCheckResult = checkContext(testFormat,
                                                               version,
                                                               QSurfaceFormat::CoreProfile);
        if (contextCheckResult.valid) {
            coreResult = contextCheckResult;
            MMLOG_DEBUG() << "[GL Probe] Found highest supported Core version: "
                          << contextCheckResult.version;
            break;
        } else {
            it = coreVersions.erase(it);
        }
    }
    return coreResult;
}

NODISCARD std::optional<GLContextCheckResult> probeCompat(
    QSurfaceFormat &format,
    std::vector<GLVersion> versions,
    QSurfaceFormat::FormatOptions options,
    std::optional<GLContextCheckResult> coreResult)
{
    std::optional<GLContextCheckResult> compatResult = std::nullopt;

    std::vector<GLVersion> compatVersionsToTest = versions;
    if (coreResult) {
        // Filter compatVersions to only include versions <= coreResult
        compatVersionsToTest.erase(std::remove_if(compatVersionsToTest.begin(),
                                                  compatVersionsToTest.end(),
                                                  [&](const GLVersion &ver) {
                                                      return ver > coreResult->version;
                                                  }),
                                   compatVersionsToTest.end());
    }

    for (const auto &version : compatVersionsToTest) {
        format.setVersion(version.major, version.minor);
        format.setProfile(QSurfaceFormat::CompatibilityProfile);
        format.setOptions(options);
        GLContextCheckResult contextCheckResult = checkContext(format,
                                                               version,
                                                               QSurfaceFormat::CompatibilityProfile);
        if (contextCheckResult.valid) {
            if (contextCheckResult.valid) {
                compatResult = contextCheckResult;
                MMLOG_DEBUG() << "[GL Probe] Found highest supported Compat version: "
                              << compatResult->version;
                break;
            }
        }
    }
    return compatResult;
}

NODISCARD std::optional<GLContextCheckResult> probeGLES(QSurfaceFormat &testFormat,
                                                        std::vector<GLVersion> &glesVersions,
                                                        QSurfaceFormat::FormatOptions options)
{
    std::optional<GLContextCheckResult> glesResult = std::nullopt;
    for (const auto &version : glesVersions) {
        testFormat.setVersion(version.major, version.minor);
        testFormat.setRenderableType(QSurfaceFormat::OpenGLES);
        testFormat.setProfile(QSurfaceFormat::NoProfile);
        testFormat.setOptions(options);

        GLContextCheckResult contextCheckResult = checkContext(testFormat,
                                                               version,
                                                               QSurfaceFormat::NoProfile);
        if (contextCheckResult.valid) {
            glesResult = contextCheckResult;
            MMLOG_DEBUG() << "[GL Probe] Found highest supported GLES version: "
                          << contextCheckResult.version;
            break;
        }
    }
    return glesResult;
}

NODISCARD std::string getHighestGLVersion(std::optional<GLContextCheckResult> coreResult,
                                          std::optional<GLContextCheckResult> compatResult)
{
    std::string highestGLVersion;
    if (coreResult && compatResult) {
        if (coreResult->version > compatResult->version) {
            highestGLVersion = formatGLVersionString(*coreResult);
        } else {
            highestGLVersion = formatGLVersionString(*compatResult);
        }
    } else if (coreResult) {
        highestGLVersion = formatGLVersionString(*coreResult);
    } else if (compatResult) {
        highestGLVersion = formatGLVersionString(*compatResult);
    } else {
        highestGLVersion = "Fallback";
    }
    return highestGLVersion;
}

NODISCARD QSurfaceFormat getOptimalFormat(std::optional<GLContextCheckResult> result)
{
    QSurfaceFormat format;
    format.setRenderableType(QSurfaceFormat::OpenGL);
    format.setDepthBufferSize(24);
    if (result) {
        format.setVersion(3, 3);
        format.setProfile(result->isCore ? QSurfaceFormat::CoreProfile
                                         : QSurfaceFormat::CompatibilityProfile);
        QSurfaceFormat::FormatOptions options;
        if (result->isDebug) {
            options |= QSurfaceFormat::DebugContext;
        }
        if (result->isCompat) {
            options |= QSurfaceFormat::DeprecatedFunctions;
        }
        MMLOG_INFO() << "[GL Probe] Optimal running format determined: GL " << format.majorVersion()
                     << "." << format.minorVersion()
                     << " Profile: " << (result->isCore ? "Core" : "Compat")
                     << (result->isDebug ? " (Debug)" : " (NO Debug)");
    } else {
        // Fallback for optimal running format if no context was found at all
        format.setVersion(3, 3);
        format.setProfile(QSurfaceFormat::CoreProfile);
        format.setOptions(QSurfaceFormat::DebugContext);
        MMLOG_ERROR() << "[GL Probe] No suitable GL context found for running format.";
    }
    return format;
}

NODISCARD OpenGLProbeResult probeOpenGLFormats()
{
    QSurfaceFormat format;
    format.setRenderableType(QSurfaceFormat::OpenGL);
    format.setDepthBufferSize(24);

    QSurfaceFormat::FormatOptions optionsCompat = QSurfaceFormat::DebugContext
                                                  | QSurfaceFormat::DeprecatedFunctions;
    QSurfaceFormat::FormatOptions optionsCore = QSurfaceFormat::DebugContext;

    // Define lists of versions to try.
    std::vector<GLVersion> versions
        = {{4, 6}, {4, 5}, {4, 4}, {4, 3}, {4, 2}, {4, 1}, {4, 0}, {3, 3}, {3, 2}};
    std::vector<GLVersion> glesVersions = {{3, 2}, {3, 1}, {3, 0}};

    std::optional<GLContextCheckResult> coreResult = probeCore(format, versions, optionsCore);
    std::optional<GLContextCheckResult> compatResult = probeCompat(format,
                                                                   versions,
                                                                   optionsCompat,
                                                                   coreResult);
    std::optional<GLContextCheckResult> glesResult = probeGLES(format, glesVersions, optionsCore);

    OpenGLProbeResult result;
    result.highestVersion = getHighestGLVersion(coreResult, compatResult);

    if (compatResult) {
        result.backend = Backend::GL33;
        result.runningFormat = getOptimalFormat(compatResult);
        result.valid = true;
    } else if (coreResult) {
        result.backend = Backend::GL33;
        result.runningFormat = getOptimalFormat(coreResult);
        result.valid = true;
    } else if (glesResult && glesResult->version.major >= 3) {
        result.backend = Backend::ES30;
        result.runningFormat = getOptimalFormat(glesResult);
        result.valid = true;
    }

    return result;
}

} // namespace

static bool setSurfaceFormat()
{
    // Probe for supported OpenGL versions
    OpenGLProbeResult probeResult = probeOpenGLFormats();
    if (!probeResult.valid) {
        return false;
    }
    g_backend = probeResult.backend;
    OpenGL::g_highest_reportable_version_string = probeResult.highestVersion;
    QSurfaceFormat fmt = probeResult.runningFormat;

    const auto &config = getConfig().canvas;
    fmt.setSamples(config.antialiasingSamples);
    QSurfaceFormat::setDefaultFormat(fmt);
    return true;
}

static void setHighDpiScaleFactorRoundingPolicy()
{
    // High Dpi is enabled by default in Qt6
    QApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
}

static void tryInitDrMingw()
{
#ifdef WITH_DRMINGW
    ExcHndlInit();
    // Set the log file path to %LocalAppData%\mmappercrash.log
    QString logFile = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
                          .replace(L'/', L'\\')
                      + QStringLiteral("\\mmappercrash.log");
    ExcHndlSetLogFileNameA(logFile.toUtf8().constData());
#endif
}

NODISCARD static bool tryLoad(MainWindow &mw, const QDir &dir, const QString &input_filename)
{
    const auto getAbsoluteFileName = [&dir, &input_filename]() -> std::optional<QString> {
        if (QFileInfo{input_filename}.isAbsolute()) {
            return input_filename;
        }

        if (!dir.exists()) {
            qInfo() << "[main] Directory" << dir.absolutePath() << "does not exist.";
            return std::nullopt;
        }

        return dir.absoluteFilePath(input_filename);
    };

    const auto maybeFilename = getAbsoluteFileName();
    if (!maybeFilename) {
        return false;
    }

    const auto &absoluteFilePath = maybeFilename.value();
    if (!QFile{absoluteFilePath}.exists()) {
        qInfo() << "[main] File " << absoluteFilePath << "does not exist.";
        return false;
    }

    mw.loadFile(absoluteFilePath);
    return true;
}

static void tryAutoLoadMap(MainWindow &mw)
{
    const auto &settings = getConfig().autoLoad;
    if (settings.autoLoadMap) {
        if (!settings.fileName.isEmpty()
            && tryLoad(mw, QDir{settings.lastMapDirectory}, settings.fileName)) {
            return;
        }
        if (!NO_MAP_RESOURCE && tryLoad(mw, QDir(":/"), "arda")) {
            return;
        }
        qInfo() << "[main] Unable to autoload map";
    }
}

int main(int argc, char **argv)
{
    setHighDpiScaleFactorRoundingPolicy();
    setEnteredMain();
    if constexpr (IS_DEBUG_BUILD) {
        // see http://doc.qt.io/qt-5/qtglobal.html#qSetMessagePattern
        // also allows environment variable QT_MESSAGE_PATTERN
        qSetMessagePattern(
            "[%{time} %{threadid}] %{type} in %{function} (at %{file}:%{line}): %{message}");
    }

    QApplication app(argc, argv);
    tryInitDrMingw();
    auto tryLoadingWinSock = std::make_unique<WinSock>();
    auto tryLoadingWinDarkMode = std::make_unique<WinDarkMode>(&app);
    if (!setSurfaceFormat()) {
        QMessageBox::critical(nullptr,
                              "OpenGL Error",
                              "No suitable OpenGL backend found. Please update your graphics drivers.");
        return 1;
    }

    tryLoadEmojis(getResourceFilenameRaw("emojis", "short-codes.json"));
    auto mw = std::make_unique<MainWindow>(g_backend);
    tryAutoLoadMap(*mw);
    const int ret = QApplication::exec();
    mw.reset();
    getConfig().write();
    return ret;
}
