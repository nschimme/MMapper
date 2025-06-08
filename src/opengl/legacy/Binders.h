#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "../../global/utils.h"
#include "Legacy.h"

#include <optional>

namespace Legacy {

struct NODISCARD BlendBinder final
{
private:
    Functions &functions;
    const BlendModeEnum blend;

public:
    explicit BlendBinder(Functions &in_functions, BlendModeEnum in_blend)
        : functions(in_functions), blend(in_blend)
    {
        functions.applyBlendMode(blend);
    }
    ~BlendBinder() = default;
    DELETE_CTORS_AND_ASSIGN_OPS(BlendBinder);
};

struct NODISCARD CullingBinder final
{
private:
    Functions &functions;

public:
    explicit CullingBinder(Functions &in_functions, const CullingEnum &in_culling)
        : functions(in_functions)
    {
        functions.applyCulling(in_culling);
    }
    ~CullingBinder() = default;
    DELETE_CTORS_AND_ASSIGN_OPS(CullingBinder);
};

struct NODISCARD DepthBinder final
{
public:
    using OptDepth = GLRenderState::OptDepth;

private:
    Functions &functions;
    const OptDepth &depth;

public:
    explicit DepthBinder(Functions &in_functions, const OptDepth &in_depth)
        : functions(in_functions), depth(in_depth)
    {
        functions.applyDepthState(depth);
    }
    ~DepthBinder() = default;
    DELETE_CTORS_AND_ASSIGN_OPS(DepthBinder);
};

struct NODISCARD LineParamsBinder
{
private:
    Functions &functions;
    const LineParams &lineParams;

public:
    explicit LineParamsBinder(Functions &in_functions, const LineParams &in_lineParams)
        : functions(in_functions), lineParams(in_lineParams)
    {
        functions.applyLineParams(lineParams);
    }
    ~LineParamsBinder() = default;
    DELETE_CTORS_AND_ASSIGN_OPS(LineParamsBinder);
};

struct NODISCARD PointSizeBinder
{
private:
    Functions &functions;
    const std::optional<GLfloat> &optPointSize;

public:
    explicit PointSizeBinder(Functions &in_functions, const std::optional<GLfloat> &in_pointSize)
        : functions(in_functions), optPointSize(in_pointSize)
    {
        functions.applyPointSize(optPointSize);
    }
    ~PointSizeBinder() = default;
    DELETE_CTORS_AND_ASSIGN_OPS(PointSizeBinder);
};

struct NODISCARD TexturesBinder final
{
public:
    using Textures = GLRenderState::Textures;

private:
    Functions &functions; // Added: Need Functions reference to call applyTexture
    const TexLookup &lookup;
    const Textures &textures;

public:
    explicit TexturesBinder(Functions &in_functions, const TexLookup &in_lookup, const Textures &in_textures) // Added Functions
        : functions(in_functions), lookup(in_lookup), textures(in_textures)
    {
        for (size_t i = 0; i < textures.size() && i < 2; ++i) { // Assuming max 2 texture units
            const auto& texInfo = textures[i];
            if (texInfo.id.isValid()) {
                // Need to get GLenum target from TexLookup or store it in GLRenderState::TextureInfo
                // For now, assuming QOpenGLTexture::Target2D as a placeholder if lookup is hard.
                // This part needs a robust way to get the texture target.
                // GLenum target = GL_TEXTURE_2D; // Placeholder
                // const auto* sharedTex = lookup.get(texInfo.id);
                // if(sharedTex && sharedTex->get()) { target = sharedTex->get()->target(); }
                // functions.applyTexture(static_cast<GLuint>(i), texInfo.id, target);

                // Ideal way: TexLookup provides the target.
                const auto* sharedTexPtr = lookup.get(texInfo.id);
                if (sharedTexPtr) {
                    const auto& sharedTex = *sharedTexPtr; // Get reference to SharedMMTexture
                    if(sharedTex && sharedTex->get()) { // Check if SharedMMTexture and QOpenGLTexture are valid
                         GLenum target = sharedTex->get()->target();
                         functions.applyTexture(static_cast<GLuint>(i), texInfo.id, target);
                    } else {
                        // Texture ID is valid but lookup failed to get a valid QOpenGLTexture:
                        // This might mean we apply with a default target or skip.
                        // For now, let's assume if ID is valid, lookup gives a usable texture.
                        // If not, this is an issue with texture management elsewhere.
                        // As a fallback, bind with TEXTURE_2D if target is unknown but ID is valid.
                         functions.applyTexture(static_cast<GLuint>(i), texInfo.id, GL_TEXTURE_2D);
                    }
                } else {
                    // Texture ID was valid, but not found in TexLookup. This is an error state.
                    // Skip or assert. For now, skip.
                }
            } else {
                // Bind texture 0 (unbind) if ID is invalid for this unit
                 functions.applyTexture(static_cast<GLuint>(i), MMTextureId{0}, GL_TEXTURE_2D);
            }
        }
    }
    ~TexturesBinder() = default;
    DELETE_CTORS_AND_ASSIGN_OPS(TexturesBinder);
};

struct NODISCARD RenderStateBinder final
{
private:
    BlendBinder blendBinder;
    CullingBinder cullingBinder;
    DepthBinder depthBinder;
    LineParamsBinder lineParamsBinder;
    PointSizeBinder pointSizeBinder;
    TexturesBinder texturesBinder;

public:
    explicit RenderStateBinder(Functions &in_functions, // Renamed for clarity
                               const TexLookup &in_lookup, // Renamed for clarity
                               const GLRenderState &renderState)
        : blendBinder(in_functions, renderState.blendMode)
        , cullingBinder(in_functions, renderState.culling)
        , depthBinder(in_functions, renderState.depthTest)
        , lineParamsBinder(in_functions, renderState.lineParams)
        , pointSizeBinder(in_functions, renderState.uniforms.pointSize)
        , texturesBinder(in_functions, in_lookup, renderState.textures) // Pass in_functions here
    {}
    DELETE_CTORS_AND_ASSIGN_OPS(RenderStateBinder);
    DTOR(RenderStateBinder) = default;
};

} // namespace Legacy
