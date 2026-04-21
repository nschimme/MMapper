// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#include "OpenGLProber.h"

#include "../global/ConfigConsts.h"
#include "../global/TextUtils.h"
#include "../global/logging.h"
#include "OpenGLConfig.h"

#include <QCoreApplication>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#ifndef EMSCRIPTEN
#include <QProcess>
#endif
#include <QString>

#ifdef WIN32
#include <windows.h>

extern "C" {
// Prefer discrete nVidia and AMD GPUs by default on Windows
__declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;
__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}
#endif

namespace {

OpenGLProber::ProbeResult getFallbackResult()
{
    OpenGLProber::ProbeResult result;
    result.backendType = OpenGLProber::BackendType::GL;
    result.format.setRenderableType(QSurfaceFormat::OpenGL);
    result.format.setVersion(3, 3);
    result.format.setProfile(QSurfaceFormat::CoreProfile);
    result.format.setDepthBufferSize(24);
    result.highestVersionString = "Fallback";
    return result;
}

} // namespace

OpenGLProber::ProbeResult OpenGLProber::probe()
{
#ifdef EMSCRIPTEN
    MMLOG_DEBUG() << "Probing for OpenGL support (Wasm/WebGL 2.0)...";
    ProbeResult result;
    result.backendType = BackendType::GLES;
    result.format.setRenderableType(QSurfaceFormat::OpenGLES);
    result.format.setVersion(3, 0);
    result.format.setDepthBufferSize(24);
    result.highestVersionString = "WebGL 2.0";
    OpenGLConfig::setESVersionString(result.highestVersionString);
    return result;
#else
    if constexpr (NO_OPENGL && NO_GLES) {
        return {};
    }

    MMLOG_DEBUG() << "Probing for OpenGL support via mmapper-hardware-survey...";

    QString surveyPath = QCoreApplication::applicationDirPath() + "/mmapper-hardware-survey";
#ifdef WIN32
    surveyPath += ".exe";
#endif

    if (!QFile::exists(surveyPath)) {
        MMLOG_ERROR() << "mmapper-hardware-survey not found at" << mmqt::toStdStringUtf8(surveyPath).c_str()
                      << ". Using fallback.";
        return getFallbackResult();
    }

    QProcess process;
    process.start(surveyPath, QStringList());
    if (!process.waitForFinished(5000)) {
        MMLOG_ERROR() << "mmapper-hardware-survey timed out or failed to start. Using fallback.";
        process.kill();
        return getFallbackResult();
    }

    QByteArray output = process.readAllStandardOutput();
    QJsonDocument doc = QJsonDocument::fromJson(output);
    if (doc.isNull() || !doc.isObject()) {
        MMLOG_ERROR() << "mmapper-hardware-survey returned invalid JSON. Using fallback.";
        return getFallbackResult();
    }

    QJsonObject obj = doc.object();
    QString backend = obj["backend"].toString();

    ProbeResult result;
    if (backend == "GL") {
        result.backendType = BackendType::GL;
        int major = obj["major"].toInt();
        int minor = obj["minor"].toInt();
        result.format.setRenderableType(QSurfaceFormat::OpenGL);
        result.format.setVersion(major, minor);
        result.format.setProfile(QSurfaceFormat::CoreProfile);
        result.format.setDepthBufferSize(24);
        result.highestVersionString = mmqt::toStdStringUtf8(QString("GL%1.%2core").arg(major).arg(minor));
        OpenGLConfig::setGLVersionString(result.highestVersionString);
    } else if (backend == "GLES") {
        result.backendType = BackendType::GLES;
        int major = obj["major"].toInt();
        int minor = obj["minor"].toInt();
        result.format.setRenderableType(QSurfaceFormat::OpenGLES);
        result.format.setVersion(major, minor);
        result.format.setDepthBufferSize(24);
        result.highestVersionString = mmqt::toStdStringUtf8(QString("ES%1.%2").arg(major).arg(minor));
        OpenGLConfig::setESVersionString(result.highestVersionString);
    } else {
        MMLOG_DEBUG() << "No suitable backend found by survey.";
        return {};
    }

    MMLOG_INFO() << "[GL Probe] Survey determined:" << result.highestVersionString.c_str();
    return result;
#endif
}
