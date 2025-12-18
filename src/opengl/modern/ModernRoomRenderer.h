#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2023 The MMapper Authors

#include <memory>
#include <vector>

#include <glm/glm.hpp>

#include "../../display/MapBatches.h"
#include "../OpenGL.h"
#include "RoomInstanceData.h"
#include "../OpenGLTypes.h"

namespace modern
{

class RoomRenderer : public IRenderable
{
public:
    RoomRenderer(OpenGL &gl, const std::vector<RoomInstanceData> &instances);
    ~RoomRenderer() final;

    void setTexture(MMTextureId textureId);

private:
    void virt_clear() final;
    void virt_reset() final;
    bool virt_isEmpty() const final;
    void virt_render(const GLRenderState &renderState) final;

private:
    class Private;
    std::unique_ptr<Private> d;
};

} // namespace modern
