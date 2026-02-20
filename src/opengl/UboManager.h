#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "../global/EnumIndexedArray.h"
#include "../global/RuleOf5.h"
#include "../global/utils.h"
#include "legacy/Legacy.h"
#include "legacy/VBO.h"

#include <bitset>
#include <cassert>
#include <functional>
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

    UboManager()
    {
        m_dirtyBlocks.set(); // All blocks are dirty initially
        m_boundBuffers.for_each([](GLuint &v) { v = 0; });
    }
    DELETE_CTORS_AND_ASSIGN_OPS(UboManager);

public:
    /**
     * @brief Marks a UBO block as dirty.
     */
    void invalidate(Legacy::SharedVboEnum block) { m_dirtyBlocks.set(static_cast<size_t>(block)); }

    /**
     * @brief Marks all UBO blocks as dirty.
     */
    void invalidateAll() { m_dirtyBlocks.set(); }

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
    bool isInvalid(Legacy::SharedVboEnum block) const
    {
        return m_dirtyBlocks.test(static_cast<size_t>(block));
    }

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

        upload_internal(gl, vbo.get(), data);
        m_dirtyBlocks.reset(static_cast<size_t>(block));

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
    template<typename T>
    void upload_internal(Legacy::Functions &gl, GLuint vbo, const T &data)
    {
        if constexpr (utils::is_vector_v<T>) {
            std::ignore = gl.setUbo(vbo, data, BufferUsageEnum::DYNAMIC_DRAW);
        } else {
            gl.setUboSingle(vbo, data, BufferUsageEnum::DYNAMIC_DRAW);
        }
    }

    void bind_internal(Legacy::Functions &gl, Legacy::SharedVboEnum block, GLuint buffer)
    {
        if (m_boundBuffers[block] != buffer) {
            gl.glBindBufferBase(GL_UNIFORM_BUFFER, block, buffer);
            m_boundBuffers[block] = buffer;
        }
    }

private:
    std::bitset<Legacy::NUM_SHARED_VBOS> m_dirtyBlocks;
    EnumIndexedArray<RebuildFunction, Legacy::SharedVboEnum> m_rebuildFunctions;
    EnumIndexedArray<GLuint, Legacy::SharedVboEnum> m_boundBuffers;
};
