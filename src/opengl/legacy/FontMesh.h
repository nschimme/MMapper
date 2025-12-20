#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2024 The MMapper Authors

#include "InstancedMesh.h"
#include "Shaders.h"

namespace Legacy {

class FontMesh final : public InstancedMesh {
public:
    explicit FontMesh(const SharedFunctions& functions, const std::shared_ptr<FontShader>& sharedShader);
    ~FontMesh() override;

    DELETE_CTORS_AND_ASSIGN_OPS(FontMesh);

    void update(const std::vector<FontData>& fontData);

private:
    void virt_bind_attributes() override;
    void virt_render(const GLRenderState& renderState) override;

    std::shared_ptr<FontShader> m_shader;
    mutable GLint m_posAttr = -1;
    mutable GLint m_sizeAttr = -1;
    mutable GLint m_texTopLeftAttr = -1;
    mutable GLint m_texBottomRightAttr = -1;
    mutable GLint m_colorAttr = -1;
    mutable GLint m_italicsAttr = -1;
    mutable GLint m_rotationAttr = -1;
};

} // namespace Legacy
