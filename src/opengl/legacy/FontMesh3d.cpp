// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "FontMesh3d.h"

#include "../../display/Textures.h"

#include <memory>
#include <vector>

namespace Legacy {

FontMesh3d::FontMesh3d(const SharedFunctions &functions,
                       const std::shared_ptr<FontShader> &sharedShader,
                       const MMTextureId textureId,
                       const DrawModeEnum mode,
                       const std::vector<FontInstanceData> &verts)
    : Base{functions, sharedShader, mode, verts}
    , m_textureId{textureId}
{}

FontMesh3d::~FontMesh3d() = default;

GLRenderState FontMesh3d::virt_modifyRenderState(const GLRenderState &renderState) const
{
    return renderState.withBlend(BlendModeEnum::TRANSPARENCY)
        .withDepthFunction(std::nullopt)
        .withDprScale(1.0f)
        .withTexture0(m_textureId);
}

} // namespace Legacy
