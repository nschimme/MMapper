#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2024 The MMapper Authors

#include "Functions.h"

namespace Legacy {

class FunctionsES30 final : public Functions
{
public:
    explicit FunctionsES30(Badge<Functions>);

private:
    void enableProgramPointSize(bool enable) override;
    NODISCARD bool tryEnableMultisampling(int requestedSamples) override;
    NODISCARD bool canRenderQuads() override;
    NODISCARD std::optional<GLenum> toGLenum(DrawModeEnum mode) override;
    NODISCARD const char *getShaderVersion() override;
};

} // namespace Legacy
