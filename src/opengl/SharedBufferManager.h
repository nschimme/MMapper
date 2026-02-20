#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "../global/EnumIndexedArray.h"
#include "../global/RuleOf5.h"
#include "../global/utils.h"
#include "legacy/Legacy.h"
#include "legacy/VBO.h"

#include <cassert>
#include <functional>
#include <optional>
#include <type_traits>
#include <vector>

namespace Legacy {

/**
 * @brief Central manager for shared Buffer Objects (UBOs, IBOs, etc.).
 *
 * Tracks which buffers are currently valid on the GPU and coordinates their updates.
 * Follows a lazy rebuild pattern: Buffers are only updated when a bind() is requested
 * and the block is marked as dirty (represented by std::nullopt in the bound buffer tracker).
 */
class SharedBufferManager final
{
public:
    using RebuildFunction = std::function<void(Legacy::Functions &gl)>;

    SharedBufferManager() { invalidateAll(); }
    DELETE_CTORS_AND_ASSIGN_OPS(SharedBufferManager);

public:
    /**
     * @brief Marks a buffer block as dirty by resetting its bound state.
     */
    void invalidate(Legacy::SharedVboEnum block) { m_boundBuffers[block] = std::nullopt; }

    /**
     * @brief Marks all buffer blocks as dirty.
     */
    void invalidateAll()
    {
        m_boundBuffers.for_each([](std::optional<GLuint> &v) { v = std::nullopt; });
    }

    /**
     * @brief Registers a function that can rebuild the buffer data.
     */
    void registerRebuildFunction(Legacy::SharedVboEnum block, RebuildFunction func)
    {
        m_rebuildFunctions[block] = std::move(func);
    }

    /**
     * @brief Checks if a buffer block is currently dirty/invalid.
     */
    bool isInvalid(Legacy::SharedVboEnum block) const { return !m_boundBuffers[block].has_value(); }

    /**
     * @brief Rebuilds the buffer if it's invalid using the registered rebuild function.
     */
    void updateIfInvalid(Legacy::Functions &gl, Legacy::SharedVboEnum block)
    {
        if (isInvalid(block)) {
            const auto &func = m_rebuildFunctions[block];
            if (func) {
                func(gl);
                // The rebuild function is expected to call update() which marks it valid.
                assert(!isInvalid(block));
            }
        }
    }

    /**
     * @brief Uploads data to the buffer and marks it as valid.
     * Also binds it to its assigned point or target.
     */
    template<typename T>
    void update(Legacy::Functions &gl, Legacy::SharedVboEnum block, const T &data)
    {
        const auto sharedVbo = gl.getSharedVbos().get(block);
        Legacy::VBO &vbo = deref(sharedVbo);

        if (!vbo) {
            vbo.emplace(gl.shared_from_this());
        }

        gl.setSharedBuffer(block, vbo.get(), data);

        bind_internal(gl, block, vbo.get());
    }

    /**
     * @brief Binds the buffer to its assigned point or target.
     * If invalid and a rebuild function is registered, it will be updated first.
     */
    void bind(Legacy::Functions &gl, Legacy::SharedVboEnum block)
    {
        updateIfInvalid(gl, block);

        const auto sharedVbo = gl.getSharedVbos().get(block);
        Legacy::VBO &vbo = deref(sharedVbo);

        if (!vbo) {
            vbo.emplace(gl.shared_from_this());
        }

        bind_internal(gl, block, vbo.get());
    }

private:
    void bind_internal(Legacy::Functions &gl, Legacy::SharedVboEnum block, GLuint buffer)
    {
        auto &bound = m_boundBuffers[block];
        if (!bound.has_value() || bound.value() != buffer) {
            const GLenum target = Legacy::getTarget(block);
            if (target == GL_UNIFORM_BUFFER) {
                gl.glBindBufferBase(target, block, buffer);
            } else {
                gl.glBindBuffer(target, buffer);
            }
            bound = buffer;
        }
    }

private:
    EnumIndexedArray<RebuildFunction, Legacy::SharedVboEnum> m_rebuildFunctions;
    EnumIndexedArray<std::optional<GLuint>, Legacy::SharedVboEnum> m_boundBuffers;
};

} // namespace Legacy
