#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "../global/RuleOf5.h"
#include "legacy/Legacy.h"
#include "legacy/VBO.h"

#include <set>
#include <type_traits>
#include <vector>

/**
 * @brief Central manager for Uniform Buffer Objects (UBOs).
 *
 * Tracks which UBOs are currently valid on the GPU and coordinates their updates.
 * Follows a "Batch" like pattern: if a UBO is dirty, it is removed from the
 * valid set, and then regenerated/uploaded on the next bind request.
 */
class UboManager final
{
public:
    UboManager() = default;
    DELETE_CTORS_AND_ASSIGN_OPS(UboManager);

public:
    /**
     * @brief Marks a UBO block as dirty by removing it from the valid set.
     */
    void invalidate(Legacy::SharedVboEnum block) { m_validBlocks.erase(block); }

    /**
     * @brief Marks all UBO blocks as dirty.
     */
    void invalidateAll() { m_validBlocks.clear(); }

    /**
     * @brief Checks if a UBO block has ever been created/initialized.
     */
    bool hasVbo(Legacy::Functions &gl, Legacy::SharedVboEnum block) const
    {
        return gl.getSharedVbos().get(block)->get() != 0;
    }

    /**
     * @brief Checks if a UBO block is currently dirty/invalid.
     */
    bool isInvalid(Legacy::SharedVboEnum block) const
    {
        return m_validBlocks.find(block) == m_validBlocks.end();
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
        m_validBlocks.insert(block);

        gl.glBindBufferBase(GL_UNIFORM_BUFFER, block, vbo.get());
    }

    /**
     * @brief Binds the UBO to its assigned point.
     * Use this when you know the UBO is already valid.
     */
    void bind(Legacy::Functions &gl, Legacy::SharedVboEnum block)
    {
        const auto sharedVbo = gl.getSharedVbos().get(block);
        Legacy::VBO &vbo = deref(sharedVbo);

        if (!vbo) {
            vbo.emplace(gl.shared_from_this());
            m_validBlocks.erase(block);
        }

        gl.glBindBufferBase(GL_UNIFORM_BUFFER, block, vbo.get());
    }

private:
    template<typename T>
    void upload_internal(Legacy::Functions &gl, GLuint vbo, const std::vector<T> &data)
    {
        std::ignore = gl.setUbo(vbo, data, BufferUsageEnum::DYNAMIC_DRAW);
    }

    template<typename T>
    void upload_internal(Legacy::Functions &gl, GLuint vbo, const T &data)
    {
        // Special case for types that are not vectors - wrap them in a vector for setUbo
        // Use SFINAE or just an overload if it works.
        // Actually, the vector overload will be preferred for vectors.
        std::ignore = gl.setUbo(vbo, std::vector<T>{data}, BufferUsageEnum::DYNAMIC_DRAW);
    }

private:
    std::set<Legacy::SharedVboEnum> m_validBlocks;
};
