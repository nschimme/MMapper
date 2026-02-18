// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "UboManager.h"
#include "legacy/Legacy.h"
#include "legacy/VBO.h"

void UboManager::registerUbo(Legacy::SharedVboEnum block, UpdateFn updateFn)
{
    m_ubos[block] = {std::move(updateFn), true};
}

void UboManager::invalidate(Legacy::SharedVboEnum block)
{
    auto it = m_ubos.find(block);
    if (it != m_ubos.end()) {
        it->second.dirty = true;
    }
}

void UboManager::updateAndBind(Legacy::Functions &gl, Legacy::SharedVboEnum block)
{
    auto it = m_ubos.find(block);
    if (it == m_ubos.end()) {
        return;
    }

    auto &entry = it->second;
    const auto sharedVbo = gl.getSharedVbos().get(block);
    Legacy::VBO &vbo = deref(sharedVbo);

    if (!vbo) {
        vbo.emplace(gl.shared_from_this());
        entry.dirty = true;
    }

    if (entry.dirty) {
        entry.updateFn(gl);
        entry.dirty = false;
    }

    gl.glBindBufferBase(GL_UNIFORM_BUFFER, block, vbo.get());
}
