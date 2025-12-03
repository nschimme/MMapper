#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Nils Schimmelmann <nschimme@gmail.com> (Jahara)

#include <string>

#include <QSurfaceFormat>

class OpenGLProber
{
public:
    enum class BackendType
    {
        None,
        GL,
        GLES
    };

    struct ProbeResult
    {
        BackendType backendType = BackendType::None;
        QSurfaceFormat format;
        std::string highestVersionString;
        bool isCompat = false;
    };

public:
    OpenGLProber() = default;
    ~OpenGLProber() = default;
    OpenGLProber(const OpenGLProber &) = delete;
    OpenGLProber &operator=(const OpenGLProber &) = delete;

    NODISCARD ProbeResult probe();
};
