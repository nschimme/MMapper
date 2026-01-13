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
    void cacheAttributeLocations();

    std::shared_ptr<FontShader> m_shader;
    std::optional<GLint> m_posAttr;
    std::optional<GLint> m_sizeAttr;
    std::optional<GLint> m_texTopLeftAttr;
    std::optional<GLint> m_texBottomRightAttr;
    std::optional<GLint> m_colorAttr;
    std::optional<GLint> m_italicsAttr;
    std::optional<GLint> m_rotationAttr;
};

} // namespace Legacy
