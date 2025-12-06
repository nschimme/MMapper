#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#include "../global/RuleOf5.h"
#include "../opengl/legacy/Legacy.h"

#include <memory>

#include <QOpenGLExtraFunctions>

class OpenGL;

class Fbo
{
public:
    Fbo();
    ~Fbo();

    DELETE_CTORS_AND_ASSIGN_OPS(Fbo);

    void emplace(const Legacy::SharedFunctions &sharedFunctions, int width, int height, int samples);
    void reset();

    void bind();
    void release();
    void blit();

    NODISCARD GLuint getId() const;
    NODISCARD explicit operator bool() const;

private:
    struct State;
    std::unique_ptr<State> m_state;
};
