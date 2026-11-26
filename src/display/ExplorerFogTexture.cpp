// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "ExplorerFogTexture.h"

#include "../global/logging.h"
#include "../opengl/OpenGL.h"
#include "Textures.h"

#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QOpenGLPixelTransferOptions>
#include <QOpenGLTexture>

namespace { // anonymous

// GL_MAX_ARRAY_TEXTURE_LAYERS is not in every header Qt exposes.
constexpr GLenum MM_GL_MAX_ARRAY_TEXTURE_LAYERS = 0x88FF;

NODISCARD GLRenderState hideEverything()
{
    return GLRenderState{}.withFog(INVALID_MM_TEXTURE_ID, GLRenderState::FogUniforms{});
}

} // namespace

void ExplorerFogTexture::set(const bool active,
                             const std::shared_ptr<const ExplorerFogMask> &base,
                             const std::optional<Coordinate> shown)
{
    m_active = active;
    m_base = base;
    m_shown = shown;
}

void ExplorerFogTexture::release(OpenGL &gl)
{
    if (m_textureId != INVALID_MM_TEXTURE_ID) {
        gl.setTextureLookup(m_textureId, nullptr);
    }
    m_texture.reset();
    m_textureId = INVALID_MM_TEXTURE_ID;
    m_uploadedBase.reset();
    m_uploadedShown.reset();
}

bool ExplorerFogTexture::upload(OpenGL &gl)
{
    release(gl);
    const ExplorerFogMask &mask = *m_base;

    QOpenGLContext *const ctx = QOpenGLContext::currentContext();
    if (ctx == nullptr) {
        return false;
    }
    GLint maxSize = 0;
    GLint maxLayers = 0;
    ctx->functions()->glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxSize);
    ctx->functions()->glGetIntegerv(MM_GL_MAX_ARRAY_TEXTURE_LAYERS, &maxLayers);
    MMLOG_INFO() << "Explore fog mask: " << mask.width << "x" << mask.height << "x" << mask.depth
                 << " (" << mask.texels.size() << " bytes), GPU limits " << maxSize << " / "
                 << maxLayers << " layers";
    if (mask.width > maxSize || mask.height > maxSize || mask.depth > maxLayers) {
        MMLOG_WARNING() << "Explore fog mask is too large for this GPU; hiding the whole map.";
        return false;
    }

    auto texture = MMTexture::alloc(
        QOpenGLTexture::Target2DArray,
        [&mask](QOpenGLTexture &tex) {
            tex.setFormat(QOpenGLTexture::R8_UNorm);
            tex.setSize(mask.width, mask.height);
            tex.setLayers(mask.depth);
            tex.setMipLevels(1);
            // Nearest and no mipmaps: anything else makes the texture incomplete, and
            // texelFetch then reads zeros.
            tex.setMinMagFilters(QOpenGLTexture::Nearest, QOpenGLTexture::Nearest);
            tex.setWrapMode(QOpenGLTexture::ClampToEdge);
            tex.allocateStorage(QOpenGLTexture::Red, QOpenGLTexture::UInt8);
            // Rows are width bytes long, not a multiple of four.
            QOpenGLPixelTransferOptions options;
            options.setAlignment(1);
            // The (x, y, z, width, height, depth) overload writes only layer 0 of an array
            // texture, which would leave every other z level zero (hidden).
            tex.setData(0,
                        0,
                        mask.depth,
                        QOpenGLTexture::CubeMapPositiveX,
                        QOpenGLTexture::Red,
                        QOpenGLTexture::UInt8,
                        mask.texels.data(),
                        &options);
        },
        /*forbidUpdates=*/true);
    if (texture == nullptr || !texture->get()->isCreated()) {
        return false;
    }

    m_textureId = allocateTextureId();
    texture->setId(m_textureId);
    gl.setTextureLookup(m_textureId, texture);
    m_texture = std::move(texture);
    m_uploadedBase = m_base;
    return true;
}

void ExplorerFogTexture::patchCell(const Coordinate cell, const uint8_t value)
{
    const ExplorerFogMask &mask = *m_uploadedBase;
    if (!mask.contains(cell) || m_texture == nullptr) {
        return;
    }
    QOpenGLPixelTransferOptions options;
    options.setAlignment(1);
    // The z offset of this overload is ignored for array textures: the layer selects the level.
    m_texture->get()->setData(cell.x - mask.min.x,
                              cell.y - mask.min.y,
                              0,
                              1,
                              1,
                              1,
                              0,
                              cell.z - mask.min.z,
                              QOpenGLTexture::Red,
                              QOpenGLTexture::UInt8,
                              &value,
                              &options);
}

GLRenderState ExplorerFogTexture::prepare(OpenGL &gl)
{
    if (!m_active) {
        if (m_texture != nullptr) {
            release(gl);
        }
        m_failed = false;
        return GLRenderState{};
    }
    if (m_base == nullptr || m_base->empty()) {
        // Active, but no mask computed yet (or nothing to show): only fog, no map.
        return hideEverything();
    }

    if (m_uploadedBase != m_base) {
        m_failed = !upload(gl);
    }
    if (m_failed || m_texture == nullptr) {
        return hideEverything();
    }

    if (m_shown != m_uploadedShown) {
        // Put the old room back to what the base says, then show the new one.
        if (m_uploadedShown) {
            patchCell(*m_uploadedShown, m_uploadedBase->at(*m_uploadedShown));
        }
        if (m_shown) {
            patchCell(*m_shown, ExplorerFogMask::SHOWN);
        }
        m_uploadedShown = m_shown;
    }

    const ExplorerFogMask &mask = *m_uploadedBase;
    GLRenderState::FogUniforms fog;
    fog.min = glm::ivec3{mask.min.x, mask.min.y, mask.min.z};
    fog.size = glm::ivec3{mask.width, mask.height, mask.depth};
    return GLRenderState{}.withFog(m_textureId, fog);
}
