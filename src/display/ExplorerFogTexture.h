#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "../global/macros.h"
#include "../map/ExplorerFogMask.h"
#include "../opengl/OpenGLTypes.h"

#include <memory>
#include <optional>

class OpenGL;

// Owns the GPU copy of the Explore-mode fog mask and keeps it in step with what the canvas has
// been told. GL thread only (every call needs the context current).
//
// Fails closed: whenever the mask is missing, empty, too large for this GPU, or cannot be
// uploaded, the render state it hands out hides every map cell.
class NODISCARD ExplorerFogTexture final
{
private:
    bool m_active = false;
    std::shared_ptr<const ExplorerFogMask> m_base;
    // The player's room: shown even though the shared base was computed without it.
    std::optional<Coordinate> m_shown;

    SharedMMTexture m_texture;
    MMTextureId m_textureId = INVALID_MM_TEXTURE_ID;
    std::shared_ptr<const ExplorerFogMask> m_uploadedBase;
    std::optional<Coordinate> m_uploadedShown;
    bool m_failed = false;

private:
    void release(OpenGL &gl);
    NODISCARD bool upload(OpenGL &gl);
    void patchCell(Coordinate cell, uint8_t value);

public:
    ExplorerFogTexture() = default;

    // Cheap, any thread that owns the canvas. Takes effect at the next prepare().
    void set(bool active,
             const std::shared_ptr<const ExplorerFogMask> &base,
             std::optional<Coordinate> shown);
    NODISCARD bool isActive() const { return m_active; }

    // The base state for drawing map content: fogged while active, unchanged otherwise.
    NODISCARD GLRenderState prepare(OpenGL &gl);
    // Drops GPU objects (context loss or shutdown).
    void reset(OpenGL &gl) { release(gl); }
};
