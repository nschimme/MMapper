#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2024 The MMapper Authors

#include "Legacy.h"
#include "../OpenGLTypes.h"
#include "VAO.h"
#include "VBO.h"

namespace Legacy {

class InstancedMesh : public IRenderable {
public:
    explicit InstancedMesh(const SharedFunctions& functions);
    ~InstancedMesh() override;

    DELETE_CTORS_AND_ASSIGN_OPS(InstancedMesh);

protected:
    virtual void virt_bind_attributes() = 0;

    SharedFunctions m_functions;
    VAO m_vao;
    VBO m_vbo;
    size_t m_instanceCount = 0;

private:
    void virt_clear() final;
    void virt_reset() final;
    bool virt_isEmpty() const final;
    void virt_render(const GLRenderState& renderState) override;
};

} // namespace Legacy
