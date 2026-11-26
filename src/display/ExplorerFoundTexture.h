#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "../global/macros.h"
#include "../map/ExplorerSecrets.h"
#include "../opengl/OpenGLTypes.h"

#include <cstdint>
#include <memory>
#include <vector>

class OpenGL;

// Owns the GPU copy of the found-secret mask (see ExplorerFoundMask) that the room shader reads
// to draw a secret exit as a door once the character has found it. GL thread only.
//
// A newly found secret does not wait for the next computation of the whole mask: addPatches()
// sets its texels (one upload each), and they are kept and applied on top of every newer base,
// so a base that was computed before the find cannot undo it. Patches only ever set bits.
//
// Fails closed: without a usable mask nothing is found, and a secret exit stays a wall.
class NODISCARD ExplorerFoundTexture final
{
private:
    bool m_active = false;
    std::shared_ptr<const ExplorerFoundMask> m_base;
    // Finds that came in on their own, newest last, applied on top of the base.
    std::vector<FoundSecretTexel> m_patches;

    SharedMMTexture m_texture;
    MMTextureId m_textureId = INVALID_MM_TEXTURE_ID;
    std::shared_ptr<const ExplorerFoundMask> m_uploadedBase;
    // What the texture holds, so a patch can set one bit without reading it back.
    std::vector<uint8_t> m_contents;
    size_t m_appliedPatches = 0;
    bool m_failed = false;

private:
    void release(OpenGL &gl);
    NODISCARD bool upload(OpenGL &gl);
    void patchTexel(const FoundSecretTexel &patch);

public:
    ExplorerFoundTexture() = default;

    // Cheap. A null base (character, map or mode changed) forgets the patches as well: they
    // belong to whoever was playing.
    void set(bool active, const std::shared_ptr<const ExplorerFoundMask> &base);
    void addPatches(const std::vector<FoundSecretTexel> &patches);
    void clearPatches() { m_patches.clear(); }

    // `state` with the found-secret texture bound, or `state` unchanged when there is none.
    NODISCARD GLRenderState prepare(OpenGL &gl, const GLRenderState &state);
    void reset(OpenGL &gl) { release(gl); }
};
