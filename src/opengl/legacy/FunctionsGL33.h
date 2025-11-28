#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2024 The MMapper Authors

#include "Functions.h"

namespace Legacy {

class FunctionsGL33 final : public Functions
{
public:
    explicit FunctionsGL33(Badge<Functions>);

private:
    void enableProgramPointSize(bool enable) override;
    NODISCARD bool tryEnableMultisampling(int requestedSamples) override;
    NODISCARD bool canRenderQuads() override;
    NODISCARD std::optional<GLenum> toGLenum(DrawModeEnum mode) override;
    NODISCARD const char *getShaderVersion() override;
};

} // namespace Legacy
