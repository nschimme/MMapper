// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#include <iostream>
#include <vector>

#include <QDebug>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QOpenGLContext>
#include <QSurfaceFormat>

struct GLVersion
{
    int major;
    int minor;
};

int main(int argc, char *argv[])
{
    // We need a QGuiApplication for OpenGL context creation
    QGuiApplication app(argc, argv);

    QJsonObject result;

    // 1. Try OpenGL Core versions
    std::vector<GLVersion> coreVersions
        = {{4, 6}, {4, 5}, {4, 4}, {4, 3}, {4, 2}, {4, 1}, {4, 0}, {3, 3}};

    bool foundGL = false;
    for (const auto &v : coreVersions) {
        QSurfaceFormat format;
        format.setRenderableType(QSurfaceFormat::OpenGL);
        format.setVersion(v.major, v.minor);
        format.setProfile(QSurfaceFormat::CoreProfile);

        QOpenGLContext context;
        context.setFormat(format);
        if (context.create()) {
            QSurfaceFormat actual = context.format();
            if (actual.profile() == QSurfaceFormat::CoreProfile
                && (actual.majorVersion() > v.major
                    || (actual.majorVersion() == v.major && actual.minorVersion() >= v.minor))) {
                result["backend"] = "GL";
                result["major"] = actual.majorVersion();
                result["minor"] = actual.minorVersion();
                foundGL = true;
                break;
            }
        }
    }

    if (!foundGL) {
        // 2. Try OpenGL ES versions
        std::vector<GLVersion> glesVersions = {{3, 2}, {3, 1}, {3, 0}};
        for (const auto &v : glesVersions) {
            QSurfaceFormat format;
            format.setRenderableType(QSurfaceFormat::OpenGLES);
            format.setVersion(v.major, v.minor);

            QOpenGLContext context;
            context.setFormat(format);
            if (context.create()) {
                QSurfaceFormat actual = context.format();
                result["backend"] = "GLES";
                result["major"] = actual.majorVersion();
                result["minor"] = actual.minorVersion();
                break;
            }
        }
    }

    if (result.isEmpty()) {
        result["backend"] = "None";
    }

    QByteArray output = QJsonDocument(result).toJson(QJsonDocument::Compact);
    std::cout << output.constData() << std::endl;
    return 0;
}
