#include "Legacy.h"

#include <optional>

namespace Legacy {

bool Functions::canRenderQuads()
{
    return false; // GL_QUADS is deprecated in Core Profile
}

std::optional<GLenum> Functions::toGLenum(const DrawModeEnum mode)
{
    switch (mode) {
    case DrawModeEnum::POINTS:
        return GL_POINTS;
    case DrawModeEnum::LINES:
        return GL_LINES;
    case DrawModeEnum::TRIANGLES:
        return GL_TRIANGLES;
    // No case for QUADS that returns GL_QUADS
    // If mode is DrawModeEnum::QUADS, it will fall through to the default
    // or the explicit case below if added.
    case DrawModeEnum::QUADS: // Explicitly make QUADS an invalid mode for direct drawing
    case DrawModeEnum::INVALID:
        // This will cause an assertion in SimpleMesh::virt_render if m_drawMode
        // was not correctly converted to TRIANGLES by setVbo.
        return std::nullopt;
    }
    // Should ideally not be reached if all DrawModeEnum values are handled.
    // Adding a default to be safe, though the enum is small.
    return std::nullopt;
}

const char *Functions::getShaderVersion()
{
    return "#version 330\n\n";
}

void Functions::enableProgramPointSize(const bool enable)
{
    if (enable) {
        Base::glEnable(GL_PROGRAM_POINT_SIZE);
    } else {
        Base::glDisable(GL_PROGRAM_POINT_SIZE);
    }
}

bool Functions::tryEnableMultisampling(const int requestedSamples) {
    if (requestedSamples > 0) {
        // It's crucial that m_viewport is up-to-date here.
        // QOpenGLWidget calls initializeGL, then resizeGL, then paintGL.
        // If tryEnableMultisampling is called in initializeGL, viewport might be default.
        // If called after a resize, m_viewport is good.
        // Let's assume this will be called when viewport is sensible or after first resize.

        // Ensure m_devicePixelRatio is valid, default to 1.0f if not set, though it should be.
        float currentDevicePixelRatio = (m_devicePixelRatio > 0.0f) ? m_devicePixelRatio : 1.0f;

        GLsizei physicalWidth = static_cast<GLsizei>(std::lround(static_cast<float>(m_viewport.size.x) * currentDevicePixelRatio));
        GLsizei physicalHeight = static_cast<GLsizei>(std::lround(static_cast<float>(m_viewport.size.y) * currentDevicePixelRatio));

        if (physicalWidth == 0 || physicalHeight == 0) {
            qWarning() << "[MSAA] Attempted to enable MSAA with zero viewport dimensions (logical: "
                       << m_viewport.size.x << "x" << m_viewport.size.y
                       << ", physical: " << physicalWidth << "x" << physicalHeight
                       << ", DPR: " << currentDevicePixelRatio << "). "
                       << "Storing " << requestedSamples << " samples and deferring FBO creation until resize.";
            destroyMsaaFBO(); // Clear any previous FBO
            m_msaaSamples = requestedSamples; // Store intent
            // Return true, meaning the "request to enable" is acknowledged.
            // The FBO will be created by handleResizeForMsaaFBO when a valid size is available.
            return true;
        }

        // If dimensions are valid, proceed to create or update FBO
        // Check if FBO is already configured with the same parameters
        if (m_msaaFbo != 0 && m_msaaSamples == requestedSamples && m_msaaWidth == physicalWidth && m_msaaHeight == physicalHeight) {
            return true; // Already configured correctly
        }
        // Otherwise, (re)create the FBO
        return createMsaaFBO(physicalWidth, physicalHeight, requestedSamples);
    } else { // requestedSamples == 0 or less
        destroyMsaaFBO(); // This will also set m_msaaSamples = 0
        return true; // Successfully disabled MSAA (or request to disable acknowledged)
    }
}

} // namespace Legacy
