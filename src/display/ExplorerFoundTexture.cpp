// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "ExplorerFoundTexture.h"

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

} // namespace

void ExplorerFoundTexture::set(const bool active,
                               const std::shared_ptr<const ExplorerFoundMask> &base)
{
    m_active = active;
    if (!active || !base) {
        m_patches.clear();
    }
    m_base = base;
}

void ExplorerFoundTexture::addPatches(const std::vector<FoundSecretTexel> &patches)
{
    m_patches.insert(m_patches.end(), patches.begin(), patches.end());
}

void ExplorerFoundTexture::release(OpenGL &gl)
{
    if (m_textureId != INVALID_MM_TEXTURE_ID) {
        gl.setTextureLookup(m_textureId, nullptr);
    }
    m_texture.reset();
    m_textureId = INVALID_MM_TEXTURE_ID;
    m_uploadedBase.reset();
    m_contents.clear();
    m_appliedPatches = 0;
}

bool ExplorerFoundTexture::upload(OpenGL &gl)
{
    release(gl);
    const ExplorerFoundMask &mask = *m_base;

    QOpenGLContext *const ctx = QOpenGLContext::currentContext();
    if (ctx == nullptr) {
        return false;
    }
    GLint maxSize = 0;
    GLint maxLayers = 0;
    ctx->functions()->glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxSize);
    ctx->functions()->glGetIntegerv(MM_GL_MAX_ARRAY_TEXTURE_LAYERS, &maxLayers);
    if (mask.width > maxSize || mask.height > maxSize || mask.depth > maxLayers) {
        MMLOG_WARNING() << "Explore found-secret mask is too large for this GPU; secrets stay "
                           "walls.";
        return false;
    }

    // The base with the finds that came in on their own: those may be newer than the base.
    m_contents = mask.texels;
    for (const FoundSecretTexel &p : m_patches) {
        if (mask.contains(p.cell)) {
            m_contents[mask.indexOf(p.cell)] |= foundSecretBit(p.dir);
        }
    }

    auto texture = MMTexture::alloc(
        QOpenGLTexture::Target2DArray,
        [&mask, this](QOpenGLTexture &tex) {
            tex.setFormat(QOpenGLTexture::R8U);
            tex.setSize(mask.width, mask.height);
            tex.setLayers(mask.depth);
            tex.setMipLevels(1);
            // Integer textures are only complete with nearest filtering and no mipmaps.
            tex.setMinMagFilters(QOpenGLTexture::Nearest, QOpenGLTexture::Nearest);
            tex.setWrapMode(QOpenGLTexture::ClampToEdge);
            tex.allocateStorage(QOpenGLTexture::Red_Integer, QOpenGLTexture::UInt8);
            // Rows are width bytes long, not a multiple of four.
            QOpenGLPixelTransferOptions options;
            options.setAlignment(1);
            // The layer overload: the (x, y, z, w, h, depth) one writes layer 0 only.
            tex.setData(0,
                        0,
                        mask.depth,
                        QOpenGLTexture::CubeMapPositiveX,
                        QOpenGLTexture::Red_Integer,
                        QOpenGLTexture::UInt8,
                        m_contents.data(),
                        &options);
        },
        /*forbidUpdates=*/true);
    if (texture == nullptr || !texture->get()->isCreated()) {
        m_contents.clear();
        return false;
    }

    m_textureId = allocateTextureId();
    texture->setId(m_textureId);
    gl.setTextureLookup(m_textureId, texture);
    m_texture = std::move(texture);
    m_uploadedBase = m_base;
    m_appliedPatches = m_patches.size();
    return true;
}

void ExplorerFoundTexture::patchTexel(const FoundSecretTexel &patch)
{
    const ExplorerFoundMask &mask = *m_uploadedBase;
    if (!mask.contains(patch.cell) || m_texture == nullptr) {
        return;
    }
    uint8_t &byte = m_contents[mask.indexOf(patch.cell)];
    byte = static_cast<uint8_t>(byte | foundSecretBit(patch.dir));
    const uint8_t value = byte;

    QOpenGLPixelTransferOptions options;
    options.setAlignment(1);
    // The z offset of this overload is ignored for array textures: the layer selects the level.
    m_texture->get()->setData(patch.cell.x - mask.min.x,
                              patch.cell.y - mask.min.y,
                              0,
                              1,
                              1,
                              1,
                              0,
                              patch.cell.z - mask.min.z,
                              QOpenGLTexture::Red_Integer,
                              QOpenGLTexture::UInt8,
                              &value,
                              &options);
}

GLRenderState ExplorerFoundTexture::prepare(OpenGL &gl, const GLRenderState &state)
{
    if (!m_active || m_base == nullptr || m_base->empty()) {
        if (m_texture != nullptr) {
            release(gl);
        }
        m_failed = false;
        return state;
    }

    if (m_uploadedBase != m_base) {
        m_failed = !upload(gl);
    }
    if (m_failed || m_texture == nullptr) {
        return state;
    }

    for (; m_appliedPatches < m_patches.size(); ++m_appliedPatches) {
        patchTexel(m_patches[m_appliedPatches]);
    }

    const ExplorerFoundMask &mask = *m_uploadedBase;
    GLRenderState::FoundUniforms found;
    found.min = glm::ivec3{mask.min.x, mask.min.y, mask.min.z};
    found.size = glm::ivec3{mask.width, mask.height, mask.depth};
    return state.withFound(m_textureId, found);
}
