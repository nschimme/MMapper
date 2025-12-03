#include "FunctionsES30.h"

namespace Legacy {

FunctionsES30::FunctionsES30() = default;

FunctionsES30::~FunctionsES30() = default;

bool FunctionsES30::canRenderQuads()
{
    return false;
}

    std::optional<GLenum> toGLenum(const DrawModeEnum mode) override
    {
        switch (mode) {
        case DrawModeEnum::POINTS:
            return GL_POINTS;
        case DrawModeEnum::LINES:
            return GL_LINES;
        case DrawModeEnum::TRIANGLES:
            return GL_TRIANGLES;

        case DrawModeEnum::INVALID:
        case DrawModeEnum::QUADS:
            break;
        }

        return std::nullopt;
    }

    const char *getShaderVersion() const override
    {
        return "#version 300 es\n\nprecision highp float;\n\n";
    }

    void enableProgramPointSize(bool /* enable */) override
    {
        // nop
    }

    bool tryEnableMultisampling(int /* requestedSamples */) override
    {
        return false;
    }
};

} // namespace Legacy
