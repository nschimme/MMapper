// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Ulf Hermann <ulfonk_mennhar@gmx.de> (Alve)
// Author: Marek Krejza <krejza@gmail.com> (Caligor)
// Author: Nils Schimmelmann <nschimme@gmail.com> (Jahara)

#include "./configuration/configuration.h"
#include "./display/Filenames.h"
#include "./global/ConfigConsts-Computed.h"
#include "./global/ConfigConsts.h"
#include "./global/WinSock.h"
#include "./global/emojis.h"
#include "./mainwindow/ThemeManager.h"
#include "./mainwindow/mainwindow.h"
#include "./opengl/OpenGLConfig.h"
#include "./opengl/OpenGLProber.h"

#include <cstring>
#include <memory>
#include <optional>
#include <type_traits>

#include <QFile>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPixmap>
#include <QSettings>
#include <QUrl>
#include <QtCore>
#include <QtWidgets>

#ifdef MMAPPER_WITH_QML
#include "./mainwindow/QmlShellWindow.h"
#include "./qml/QmlTypes.h"

#include <QQuickStyle>
#include <QQuickWindow>
#endif

#ifdef WITH_DRMINGW
#include <exchndl.h>
#endif

static void setHighDpiScaleFactorRoundingPolicy()
{
    // High Dpi is enabled by default in Qt6
    QApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::Round);
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

// Templated so the same autoload flow drives both shells: Shell A
// (MainWindow) and Shell B (QmlShellWindow), which both expose
// `void loadFile(std::shared_ptr<MapSource>)` (see mainwindow/mainwindow.h
// and mainwindow/QmlShellWindow.h) as their widget-free load entry point.
// This is the "least churn" option the task called for over a ShellFacade
// wrapper type: both classes already share the exact method signature that
// matters here, so a template picks it up with no new abstraction.
template<typename Shell>
NODISCARD static bool tryLoad(Shell &shell, const QDir &dir, const QString &input_filename)
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

    try {
        shell.loadFile(MapSource::alloc(absoluteFilePath, std::nullopt));
        return true;
    } catch (const std::runtime_error &e) {
        qCritical() << "Failed to load autoload map:" << e.what();
        return false;
    }
}

template<typename Shell>
static void tryAutoLoadMap(Shell &shell)
{
    const auto &settings = getConfig().autoLoad;
    if (!settings.autoLoadMap) {
        return;
    }

    if (!settings.fileName.isEmpty()
        && tryLoad(shell, QDir{settings.lastMapDirectory}, settings.fileName)) {
        return;
    }

    if constexpr (CURRENT_PLATFORM == PlatformEnum::Wasm) {
        if constexpr (NO_MAP_RESOURCE) {
            return;
        } else {
            // On WASM the map is sideloaded from the network; fetch it
            // asynchronously. Works for either shell -- both expose
            // loadFile(std::shared_ptr<MapSource>) -- and the QML shell is
            // now the wasm default (see main()'s ShellTypeEnum comment).
            auto *nam = new QNetworkAccessManager(&shell);
            auto *reply = nam->get(QNetworkRequest(QUrl(getAssetsPath() + "map/arda")));
            QObject::connect(reply, &QNetworkReply::finished, &shell, [&shell, reply, nam]() {
                if (reply->error() == QNetworkReply::NoError) {
                    try {
                        shell.loadFile(MapSource::alloc(QStringLiteral("arda"), reply->readAll()));
                    } catch (const std::exception &e) {
                        qCritical() << "[main] Failed to load sideloaded map:" << e.what();
                    }
                } else {
                    qWarning() << "[main] Failed to fetch sideloaded map:" << reply->errorString();
                }
                reply->deleteLater();
                nam->deleteLater();
            });
        }
    } else {
        if (!NO_MAP_RESOURCE) {
            // Check the system assets directory
            if (tryLoad(shell, QDir(getAssetsPath() + "map/"), "arda")) {
                return;
            }
        }
        qInfo() << "[main] Unable to autoload map";
    }
}

#ifndef Q_OS_WASM
enum class NODISCARD ShellTypeEnum { Widgets, Qml };

// Picks between Shell A (today's QOpenGLWindow-based MainWindow, unchanged)
// and Shell B (the --qml-shell preview; see QmlShellWindow.h). Checked in
// this order, matching the --probe scan's style of a manual argv walk
// ahead of QApplication:
//   1. argv: --qml-shell / --widgets-shell
//   2. env: MMAPPER_QML_SHELL=1/0
//   3. persisted "preferred shell" setting (Configuration::general.qmlShell,
//      toggled from the QML preferences General page; see
//      GeneralPageAdapter::qmlShell / PrefsGeneralPage.qml)
//   4. default: Widgets (Shell A) -- byte-identical to pre-QML-shell
//      behavior.
//
// Source 3 is read with a raw QSettings peek rather than by constructing a
// real Configuration, because Configuration's file I/O (and its
// MMAPPER_PROFILE_PATH env-var override, first-run migration, color
// defaults, etc.) is more than this early call site needs or should trigger
// before QApplication exists. The peek below deliberately duplicates just
// the identity bits (organization/application name, "General" group, "Qml
// Shell" key) that configuration.cpp's Settings class and
// Configuration::GeneralSettings::read()/write() use for this one value --
// see SETTINGS_ORGANIZATION/SETTINGS_APPLICATION, GRP_GENERAL, and
// KEY_QML_SHELL in src/configuration/configuration.cpp. If any of those
// change, this peek must change with them. (It intentionally does NOT honor
// MMAPPER_PROFILE_PATH's custom-settings-file override -- a user who sets
// that env var and also wants shell-preference persistence can still use
// argv/MMAPPER_QML_SHELL, both of which outrank this source anyway.)
NODISCARD static bool peekPersistedQmlShellPreference()
{
    QCoreApplication::setOrganizationName(QStringLiteral("MUME"));
    QCoreApplication::setApplicationName(QStringLiteral("MMapper2"));
    try {
        const QSettings settings;
        return settings.value(QStringLiteral("General/Qml Shell"), false).toBool();
    } catch (...) {
        return false;
    }
}

NODISCARD static ShellTypeEnum determineShellType(int argc, char **argv)
{
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--qml-shell") == 0) {
            return ShellTypeEnum::Qml;
        }
        if (std::strcmp(argv[i], "--widgets-shell") == 0) {
            return ShellTypeEnum::Widgets;
        }
    }

    const QByteArray env = qgetenv("MMAPPER_QML_SHELL");
    if (env == "1") {
        return ShellTypeEnum::Qml;
    }
    if (env == "0") {
        return ShellTypeEnum::Widgets;
    }

    if (peekPersistedQmlShellPreference()) {
        return ShellTypeEnum::Qml;
    }

    return ShellTypeEnum::Widgets;
}
#endif

static bool setSurfaceFormat()
{
    OpenGLProber prober;
    auto probeResult = prober.probe();
    if (probeResult.backendType == OpenGLProber::BackendType::None) {
        QString msg = "No compatible rendering backend found.\n\nThe applications requires ";
        if constexpr (!NO_OPENGL) {
            msg += "OpenGL 3.3";
        }
        if constexpr (!NO_OPENGL && !NO_GLES) {
            msg += " or ";
        }
        if constexpr (!NO_GLES) {
            if constexpr (CURRENT_PLATFORM == PlatformEnum::Wasm) {
                msg += "WebGL 2.0";
            } else {
                msg += "GLES 3.0";
            }
        }
        msg += " support to run.";
        QMessageBox::critical(nullptr, "Fatal Error", msg);
        return false;
    }
    OpenGLConfig::setBackendType(probeResult.backendType);
    QSurfaceFormat::setDefaultFormat(probeResult.format);
    return true;
}

int main(int argc, char **argv)
{
#ifndef Q_OS_WASM
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--probe") == 0) {
            return OpenGLProber::runSurveyMode(argc, argv);
        }
    }
#endif

    setHighDpiScaleFactorRoundingPolicy();
    setEnteredMain();
    if constexpr (IS_DEBUG_BUILD) {
        // see http://doc.qt.io/qt-5/qtglobal.html#qSetMessagePattern
        // also allows environment variable QT_MESSAGE_PATTERN
        qSetMessagePattern(
            "[%{time} %{threadid}] %{type} in %{function} (at %{file}:%{line}): %{message}");
    }

    // WASM has no argv/environment to select a shell from the way a desktop
    // build does (determineShellType() isn't even compiled under
    // Q_OS_WASM). It historically ran the QQuickWidget-panels-on-MainWindow
    // path; as of this branch it defaults to the QML shell so the wasm/
    // WebGL2 build exercises the single-QQuickWindow architecture the
    // migration targets. Flip back to Widgets here if wasm testing finds a
    // blocker.
#ifdef Q_OS_WASM
    ShellTypeEnum shellType = ShellTypeEnum::Qml;
#else
    ShellTypeEnum shellType = determineShellType(argc, argv);
#endif

#ifdef MMAPPER_WITH_QML
    if (shellType == ShellTypeEnum::Qml) {
        // Shell B (--qml-shell): the map itself is, by default, drawn
        // directly into the window's own framebuffer by
        // MapCanvasUnderlayItem (see display/MapCanvasUnderlayItem.h) --
        // or, as a fallback (MMAPPER_CANVAS_FBO=1; see QmlShellWindow.cpp),
        // a genuine QQuickWindow scene-graph item owning its own FBO
        // (MapCanvasQuickItem; see display/MapCanvasQuickItem.h) -- not a
        // QOpenGLWindow composited alongside QQuickWidget docks the way
        // Shell A works -- so none of the Shell-A-only software-backend
        // rationale below applies. Use a real hardware-accelerated backend,
        // and the single-threaded ("basic") render loop both classes' file
        // comments document as their threading assumption.
        QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);
        if (qEnvironmentVariableIsEmpty("QSG_RENDER_LOOP")) {
            qputenv("QSG_RENDER_LOOP", "basic");
        }
    } else {
        // Shell A (default): QQuickWidget dock panels are forced onto the software scene-graph
        // backend. On macOS, the GL RHI backend composites via a QOpenGLContext that fights with
        // the widget backing store's own Metal/CoreAnimation compositor once the same top-level
        // window also hosts the map's native QOpenGLWindow (via createWindowContainer),
        // corrupting the backing store with uninitialized garbage. The software backend
        // renders each QQuickWidget into a QImage and blits it straight into the widget
        // backing store, so no GL context is involved at all. The panels are simple 2D UI,
        // so software rendering costs nothing noticeable. Once the map itself becomes a
        // QQuickWindow scene-graph underlay (the end state of the QML migration, with no
        // QQuickWidget in the picture), this can move back to a hardware-accelerated backend.
        QQuickWindow::setGraphicsApi(QSGRendererInterface::Software);
    }
    QQuickStyle::setStyle("Fusion");
    // Registers MapCanvasItem (and any future C++-backed QML types) with the
    // "MMapper" module.
    registerMmQmlTypes();
#else
    if (shellType == ShellTypeEnum::Qml) {
        qCritical() << "[main] --qml-shell (or MMAPPER_QML_SHELL=1) requires a build with "
                       "WITH_QML enabled; this build does not have it. Falling back to the "
                       "widgets shell.";
        shellType = ShellTypeEnum::Widgets;
    }
#endif

    QApplication app(argc, argv);
    tryInitDrMingw();
    auto tryLoadingWinSock = std::make_unique<WinSock>();
    auto themeManager = std::make_unique<ThemeManager>(&app);
    if (!setSurfaceFormat()) {
        return 1;
    }

    tryLoadEmojis(getResourceFilenameRaw("emojis", "short-codes.json"));

    std::unique_ptr<MainWindow> mw;
#ifdef MMAPPER_WITH_QML
    std::unique_ptr<QmlShellWindow> qmlShell;
#endif

    if (shellType == ShellTypeEnum::Widgets) {
        mw = std::make_unique<MainWindow>();
        tryAutoLoadMap(*mw);
    }
#ifdef MMAPPER_WITH_QML
    else {
        qmlShell = std::make_unique<QmlShellWindow>(nullptr);
        if (!qmlShell->isValid()) {
            QMessageBox::critical(nullptr,
                                  "Fatal Error",
                                  "Failed to load the QML shell (MainShell.qml).");
            return 1;
        }
        tryAutoLoadMap(*qmlShell);
    }
#endif

    const int ret = QApplication::exec();
    qDebug() << "QApplication::exec() returned" << ret;
    mw.reset();
#ifdef MMAPPER_WITH_QML
    qmlShell.reset();
#endif
    getConfig().write();
    qInfo() << "Exiting main.";
    return ret;
}
