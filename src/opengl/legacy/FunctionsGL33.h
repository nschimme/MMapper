#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2024 The MMapper Authors

#include "Legacy.h"

namespace Legacy {

class FunctionsGL33 final : public Functions
{
public:
    FunctionsGL33();
    ~FunctionsGL33() override;

private:
    void enableProgramPointSize(bool enable) override;
    NODISCARD bool tryEnableMultisampling(int requestedSamples) override;
    NODISCARD const char *getShaderVersion() const override;
    NODISCARD bool canRenderQuads() override;
    NODISCARD std::optional<GLenum> toGLenum(DrawModeEnum mode) override;
};

} // namespace Legacy
