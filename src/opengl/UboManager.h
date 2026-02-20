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

/**
 * @brief Central manager for Uniform Buffer Objects (UBOs).
 *
 * Tracks which UBOs are currently valid on the GPU and coordinates their updates.
 * Follows a lazy rebuild pattern: UBOs are only updated when a bind() is requested
 * and the block is marked as dirty.
 */
class UboManager final
{
public:
    using RebuildFunction = std::function<void(Legacy::Functions &gl)>;

    UboManager() { invalidateAll(); }
    DELETE_CTORS_AND_ASSIGN_OPS(UboManager);

public:
    /**
     * @brief Marks a UBO block as dirty.
     */
    void invalidate(Legacy::SharedVboEnum block) { m_dirtyBlocks[block] = true; }

    /**
     * @brief Marks all UBO blocks as dirty.
     */
    void invalidateAll()
    {
        m_dirtyBlocks.for_each([](bool &dirty) { dirty = true; });
    }

    /**
     * @brief Registers a function that can rebuild the UBO data.
     */
    void registerRebuildFunction(Legacy::SharedVboEnum block, RebuildFunction func)
    {
        m_rebuildFunctions[block] = std::move(func);
    }

    /**
     * @brief Checks if a UBO block is currently dirty/invalid.
     */
    bool isInvalid(Legacy::SharedVboEnum block) const { return m_dirtyBlocks[block]; }

    /**
     * @brief Rebuilds the UBO if it's invalid using the registered rebuild function.
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
     * @brief Uploads data to the UBO and marks it as valid.
     * Also binds it to its assigned point.
     */
    template<typename T>
    void update(Legacy::Functions &gl, Legacy::SharedVboEnum block, const T &data)
    {
        const auto sharedVbo = gl.getSharedVbos().get(block);
        Legacy::VBO &vbo = deref(sharedVbo);

        if (!vbo) {
            vbo.emplace(gl.shared_from_this());
        }

        static_cast<void>(gl.setUbo(vbo.get(), data, BufferUsageEnum::DYNAMIC_DRAW));
        m_dirtyBlocks[block] = false;

        bind_internal(gl, block, vbo.get());
    }

    /**
     * @brief Binds the UBO to its assigned point.
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
            gl.glBindBufferBase(GL_UNIFORM_BUFFER, block, buffer);
            bound = buffer;
        }
    }

private:
    EnumIndexedArray<bool, Legacy::SharedVboEnum> m_dirtyBlocks;
    EnumIndexedArray<RebuildFunction, Legacy::SharedVboEnum> m_rebuildFunctions;
    EnumIndexedArray<std::optional<GLuint>, Legacy::SharedVboEnum> m_boundBuffers;
};
