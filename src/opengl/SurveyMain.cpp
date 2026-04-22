// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#include <iostream>
#include <vector>

#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QOpenGLContext>
#include <QSurfaceFormat>

#ifdef WIN32
#include <windows.h>
extern "C" {
__declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;
__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}
#endif

struct GLVersion
{
    int major = 0;
    int minor = 0;

    bool operator>(const GLVersion &other) const
    {
        return (major > other.major) || (major == other.major && minor > other.minor);
    }
};

struct GLContextCheckResult
{
    bool valid = false;
    GLVersion version = {0, 0};
    bool isCore = false;
};

GLContextCheckResult checkContext(QSurfaceFormat format)
{
    GLContextCheckResult result;
    QOpenGLContext context;
    context.setFormat(format);
    if (!context.create()) {
        return result;
    }

    QSurfaceFormat actualFormat = context.format();
    result.version = {actualFormat.majorVersion(), actualFormat.minorVersion()};
    result.isCore = (actualFormat.profile() == QSurfaceFormat::CoreProfile);

    // Core profile only exists for 3.2+, but we only target 3.3+
    if (result.version.major > 3 || (result.version.major == 3 && result.version.minor >= 3)) {
        if (result.isCore) {
            result.valid = true;
        }
    } else {
        result.valid = false;
    }

    return result;
}

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);

    QJsonObject root;
    root["backend"] = "None";

    // Try OpenGL Core
    std::vector<GLVersion> versions
        = {{4, 6}, {4, 5}, {4, 4}, {4, 3}, {4, 2}, {4, 1}, {4, 0}, {3, 3}};
    bool foundGL = false;

    for (const auto &v : versions) {
        QSurfaceFormat format;
        format.setRenderableType(QSurfaceFormat::OpenGL);
        format.setVersion(v.major, v.minor);
        format.setProfile(QSurfaceFormat::CoreProfile);

        auto res = checkContext(format);
        if (res.valid) {
            root["backend"] = "GL";
            root["major"] = res.version.major;
            root["minor"] = res.version.minor;
            root["profile"] = "Core";
            foundGL = true;
            break;
        }
    }

    if (!foundGL) {
        // Try GLES
        std::vector<GLVersion> glesVersions = {{3, 2}, {3, 1}, {3, 0}};
        for (const auto &v : glesVersions) {
            QSurfaceFormat format;
            format.setRenderableType(QSurfaceFormat::OpenGLES);
            format.setVersion(v.major, v.minor);

            QOpenGLContext context;
            context.setFormat(format);
            if (context.create()) {
                root["backend"] = "GLES";
                root["major"] = v.major;
                root["minor"] = v.minor;
                break;
            }
        }
    }

    QJsonDocument doc(root);
    std::cout << doc.toJson(QJsonDocument::Compact).constData() << std::endl;

    return 0;
}
