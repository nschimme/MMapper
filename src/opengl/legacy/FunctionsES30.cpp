#include "FunctionsES30.h"

namespace Legacy {

FunctionsES30::FunctionsES30(Badge<Functions> badge)
    : Functions(badge)
{}

bool FunctionsES30::canRenderQuads()
{
    return false;
}

std::optional<GLenum> FunctionsES30::toGLenum(const DrawModeEnum mode)
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

const char *FunctionsES30::getShaderVersion()
{
    return "#version 300 es\n\nprecision highp float;\n\n";
}

void FunctionsES30::enableProgramPointSize(bool /* enable */)
{
    // nop
}

bool FunctionsES30::tryEnableMultisampling(int /* requestedSamples */)
{
    return false;
}

} // namespace Legacy
