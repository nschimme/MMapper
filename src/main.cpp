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

#include <QFile>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPixmap>
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

    try {
        mw.loadFile(MapSource::alloc(absoluteFilePath, std::nullopt));
        return true;
    } catch (const std::runtime_error &e) {
        qCritical() << "Failed to load autoload map:" << e.what();
        return false;
    }
}

static void tryAutoLoadMap(MainWindow &mw)
{
    const auto &settings = getConfig().autoLoad;
    if (!settings.autoLoadMap) {
        return;
    }

    if (!settings.fileName.isEmpty()
        && tryLoad(mw, QDir{settings.lastMapDirectory}, settings.fileName)) {
        return;
    }

    if constexpr (CURRENT_PLATFORM == PlatformEnum::Wasm) {
        if constexpr (NO_MAP_RESOURCE) {
            return;
        }
        // On WASM the map is sideloaded from the network; fetch it asynchronously.
        auto *nam = new QNetworkAccessManager(&mw);
        auto *reply = nam->get(QNetworkRequest(QUrl(getAssetsPath() + "map/arda")));
        QObject::connect(reply, &QNetworkReply::finished, &mw, [&mw, reply, nam]() {
            if (reply->error() == QNetworkReply::NoError) {
                try {
                    mw.loadFile(MapSource::alloc(QStringLiteral("arda"), reply->readAll()));
                } catch (const std::exception &e) {
                    qCritical() << "[main] Failed to load sideloaded map:" << e.what();
                }
            } else {
                qWarning() << "[main] Failed to fetch sideloaded map:" << reply->errorString();
            }
            reply->deleteLater();
            nam->deleteLater();
        });
    } else {
        if (!NO_MAP_RESOURCE) {
            // Check the system assets directory
            if (tryLoad(mw, QDir(getAssetsPath() + "map/"), "arda")) {
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
//   3. default: Widgets (Shell A) -- byte-identical to pre-QML-shell
//      behavior.
// TODO(later commit): once a persisted "preferred shell" setting exists in
// Configuration, peek it here as a fourth, lowest-priority source. Not done
// yet: reading Configuration this early would mean initializing it (and
// its file I/O) before QApplication exists, which nothing else in main()
// currently needs to do.
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
    // build does; it always ran the QQuickWidget-panels-on-MainWindow path
    // below and continues to (see determineShellType(), which isn't even
    // compiled under Q_OS_WASM).
    ShellTypeEnum shellType = ShellTypeEnum::Widgets;
#ifndef Q_OS_WASM
    shellType = determineShellType(argc, argv);
#endif

#ifdef MMAPPER_WITH_QML
    if (shellType == ShellTypeEnum::Qml) {
        // Shell B (--qml-shell): the map itself will be a genuine
        // QQuickWindow scene-graph item (MapCanvasQuickItem; see
        // display/MapCanvasQuickItem.h), not a QOpenGLWindow composited
        // alongside QQuickWidget docks the way Shell A works -- so none of
        // the Shell-A-only software-backend rationale below applies. Use a
        // real hardware-accelerated backend, and the single-threaded
        // ("basic") render loop MapCanvasQuickItem's file comment documents
        // as its threading assumption.
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
