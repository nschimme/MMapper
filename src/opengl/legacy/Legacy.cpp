// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "Legacy.h"

#include "../../display/Textures.h"
#include "../../global/utils.h"
#include "../OpenGLTypes.h"
#include "AbstractShaderProgram.h"
#include "Binders.h"
#include "FontMesh3d.h"
#include "Meshes.h"
#include "ShaderUtils.h"
#include "Shaders.h"
#include "SimpleMesh.h"
#include "VBO.h"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <QDebug>
#include <QFile>
#include <QMessageLogContext>
#include <QOpenGLTexture>

namespace Legacy {
template<template<typename> typename Mesh_, typename VertType_, typename ProgType_>
NODISCARD static auto createMesh(const SharedFunctions &functions,
                                 const DrawModeEnum mode,
                                 const std::vector<VertType_> &batch,
                                 const std::shared_ptr<ProgType_> &prog)
{
    using Mesh = Mesh_<VertType_>;
    static_assert(std::is_same_v<typename Mesh::ProgramType, ProgType_>);
    return std::make_unique<Mesh>(functions, prog, mode, batch);
}

template<template<typename> typename Mesh_, typename VertType_, typename ProgType_>
NODISCARD static UniqueMesh createUniqueMesh(const SharedFunctions &functions,
                                             const DrawModeEnum mode,
                                             const std::vector<VertType_> &batch,
                                             const std::shared_ptr<ProgType_> &prog)
{
    assert(mode != DrawModeEnum::INVALID);
    return UniqueMesh(createMesh<Mesh_, VertType_, ProgType_>(functions, mode, batch, prog));
}

template<template<typename> typename Mesh_, typename VertType_, typename ProgType_>
NODISCARD static UniqueMesh createTexturedMesh(const SharedFunctions &functions,
                                               const DrawModeEnum mode,
                                               const std::vector<VertType_> &batch,
                                               const std::shared_ptr<ProgType_> &prog,
                                               const MMTextureId texture)
{
    assert(static_cast<size_t>(mode) >= VERTS_PER_TRI);
    return UniqueMesh{std::make_unique<TexturedRenderable>(
        texture, createMesh<Mesh_, VertType_, ProgType_>(functions, mode, batch, prog))};
}

UniqueMesh Functions::createPointBatch(const std::vector<ColorVert> &batch)
{
    const auto &prog = getShaderPrograms().getPointShader();
    return createUniqueMesh<PointMesh>(shared_from_this(), DrawModeEnum::POINTS, batch, prog);
}

UniqueMesh Functions::createPlainBatch(const DrawModeEnum mode, const std::vector<glm::vec3> &batch)
{
    assert(static_cast<size_t>(mode) >= VERTS_PER_LINE);
    const auto &prog = getShaderPrograms().getPlainUColorShader();
    return createUniqueMesh<PlainMesh>(shared_from_this(), mode, batch, prog);
}

UniqueMesh Functions::createColoredBatch(const DrawModeEnum mode,
                                         const std::vector<ColorVert> &batch)
{
    assert(static_cast<size_t>(mode) >= VERTS_PER_LINE);
    const auto &prog = getShaderPrograms().getPlainAColorShader();
    return createUniqueMesh<ColoredMesh>(shared_from_this(), mode, batch, prog);
}

UniqueMesh Functions::createTexturedBatch(const DrawModeEnum mode,
                                          const std::vector<TexVert> &batch,
                                          const MMTextureId texture)
{
    assert(static_cast<size_t>(mode) >= VERTS_PER_TRI);
    const auto &prog = getShaderPrograms().getTexturedUColorShader();
    return createTexturedMesh<TexturedMesh>(shared_from_this(), mode, batch, prog, texture);
}

UniqueMesh Functions::createColoredTexturedBatch(const DrawModeEnum mode,
                                                 const std::vector<ColoredTexVert> &batch,
                                                 const MMTextureId texture)
{
    assert(static_cast<size_t>(mode) >= VERTS_PER_TRI);
    const auto &prog = getShaderPrograms().getTexturedAColorShader();
    return createTexturedMesh<ColoredTexturedMesh>(shared_from_this(), mode, batch, prog, texture);
}

template<typename VertexType_, template<typename> typename Mesh_, typename ShaderType_>
static void renderImmediate(const SharedFunctions &sharedFunctions,
                            const DrawModeEnum mode,
                            const std::vector<VertexType_> &verts,
                            const std::shared_ptr<ShaderType_> &sharedShader,
                            const GLRenderState &renderState)
{
    if (verts.empty()) {
        return;
    }

    static WeakVbo weak;
    auto shared = weak.lock();
    if (shared == nullptr) {
        weak = shared = sharedFunctions->getStaticVbos().alloc();
        if (shared == nullptr) {
            throw std::runtime_error("OpenGL error: failed to alloc VBO");
        }
    }

    VBO &vbo = *shared;
    if (!vbo) {
        vbo.emplace(sharedFunctions);
    }

    using Mesh = Mesh_<VertexType_>;
    static_assert(std::is_same_v<typename Mesh::ProgramType, ShaderType_>);

    const auto before = vbo.get();
    {
        Mesh mesh{sharedFunctions, sharedShader};
        {
            // temporarily loan our VBO to the mesh.
            mesh.unsafe_swapVboId(vbo);
            assert(!vbo);
            {
                mesh.setDynamic(mode, verts);
                mesh.render(renderState);
            }
            mesh.unsafe_swapVboId(vbo);
            assert(vbo);
        }
    }
    const auto after = vbo.get();
    assert(before == after);
    sharedFunctions->clearVbo(vbo.get());
}

void Functions::renderPlain(const DrawModeEnum mode,
                            const std::vector<glm::vec3> &verts,
                            const GLRenderState &state)
{
    assert(static_cast<size_t>(mode) >= VERTS_PER_LINE);
    const auto &shared = shared_from_this();
    const auto &prog = getShaderPrograms().getPlainUColorShader();
    renderImmediate<glm::vec3, Legacy::PlainMesh>(shared, mode, verts, prog, state);
}

void Functions::renderColored(const DrawModeEnum mode,
                              const std::vector<ColorVert> &verts,
                              const GLRenderState &state)
{
    assert(static_cast<size_t>(mode) >= VERTS_PER_LINE);
    const auto &prog = getShaderPrograms().getPlainAColorShader();
    renderImmediate<ColorVert, Legacy::ColoredMesh>(shared_from_this(), mode, verts, prog, state);
}

void Functions::renderPoints(const std::vector<ColorVert> &verts, const GLRenderState &state)
{
    assert(state.uniforms.pointSize);
    const auto &prog = getShaderPrograms().getPointShader();
    renderImmediate<ColorVert, Legacy::PointMesh>(shared_from_this(),
                                                  DrawModeEnum::POINTS,
                                                  verts,
                                                  prog,
                                                  state);
}

void Functions::renderTextured(const DrawModeEnum mode,
                               const std::vector<TexVert> &verts,
                               const GLRenderState &state)
{
    assert(static_cast<size_t>(mode) >= VERTS_PER_TRI);
    const auto &prog = getShaderPrograms().getTexturedUColorShader();
    renderImmediate<TexVert, Legacy::TexturedMesh>(shared_from_this(), mode, verts, prog, state);
}

void Functions::renderColoredTextured(const DrawModeEnum mode,
                                      const std::vector<ColoredTexVert> &verts,
                                      const GLRenderState &state)
{
    assert(static_cast<size_t>(mode) >= VERTS_PER_TRI);
    const auto &prog = getShaderPrograms().getTexturedAColorShader();
    renderImmediate<ColoredTexVert, Legacy::ColoredTexturedMesh>(shared_from_this(),
                                                                 mode,
                                                                 verts,
                                                                 prog,
                                                                 state);
}

void Functions::renderFont3d(const SharedMMTexture &texture, const std::vector<FontVert3d> &verts)

{
    const auto state = GLRenderState()
                           .withBlend(BlendModeEnum::TRANSPARENCY)
                           .withDepthFunction(std::nullopt)
                           .withTexture0(texture->getId());

    const auto &prog = getShaderPrograms().getFontShader();
    renderImmediate<FontVert3d, Legacy::SimpleFont3dMesh>(shared_from_this(),
                                                          DrawModeEnum::QUADS,
                                                          verts,
                                                          prog,
                                                          state);
}

UniqueMesh Functions::createFontMesh(const SharedMMTexture &texture,
                                     const DrawModeEnum mode,
                                     const std::vector<FontVert3d> &batch)
{
    assert(static_cast<size_t>(mode) >= VERTS_PER_TRI);
    const auto &prog = getShaderPrograms().getFontShader();
    return UniqueMesh{
        std::make_unique<Legacy::FontMesh3d>(shared_from_this(), prog, texture, mode, batch)};
}

Functions::Functions(Badge<Functions>)
    : m_shaderPrograms{std::make_unique<ShaderPrograms>(*this)}
    , m_staticVbos{std::make_unique<StaticVbos>()}
    , m_texLookup{std::make_unique<TexLookup>()}
    // Initialize tracked states to OpenGL defaults
    , m_currentBlendMode{BlendModeEnum::NONE}
    , m_blendEnabled{false}
    , m_currentDepthFunction{DepthFunctionEnum::LESS} // Default OpenGL depth func is GL_LESS
    , m_depthTestEnabled{false} // Depth test is disabled by default
    , m_currentShaderProgramId{0} // No program bound by default
    , m_currentLineParams{1.0f} // Default line width
    , m_currentPointSize{std::nullopt} // Point size state is more complex, default to not set
    , m_currentCullingMode{CullingEnum::BACK} // Default cull face is GL_BACK
    , m_cullingEnabled{false} // Culling is disabled by default
{
    // Initialize texture units state
    for (size_t i = 0; i < m_currentTextureIds.size(); ++i) {
        m_currentTextureIds[i] = MMTextureId{0}; // No texture bound (ID 0)
        m_currentTextureTargets[i] = GL_TEXTURE_2D; // Default target, though irrelevant if ID is 0
    }
    // It's good practice to ensure the active texture unit is reset to 0 if changed during init,
    // though QOpenGLFunctions usually manages this well.
    // For explicit safety for texture unit 1, if m_currentTextureIds.size() > 1:
    if (m_currentTextureIds.size() > 1) {
         Base::glActiveTexture(GL_TEXTURE1); // Ensure we are talking about texture unit 1
         Base::glBindTexture(GL_TEXTURE_2D, 0); // Bind texture 0 to unit 1
         // Bind other targets to 0 as well if necessary, e.g. GL_TEXTURE_CUBE_MAP
         Base::glActiveTexture(GL_TEXTURE0); // Reset active texture unit to 0
    }
}

Functions::~Functions()
{
    cleanup();
}

/// <ul>
/// <li>Resets the Wrapped GL's cached copies of (compiled) shaders given out
/// to new meshes. This <em>does NOT</em> expire the shaders belonging to old
/// mesh objects, so that means it's possible to end up with a mixture of old
/// and new mesh objects each with different instances of the same shader
/// program. (In other words: if you want to add shader hot-reloading, then
/// instead of calling this function you'll probably want to just immediately
/// recompile the old shaders.) </li>
///
/// <li>Resets shared pointers to VBOs owned by this object but given out on
/// "extended loan" to static immediate-rendering functions. Those functions
/// only keep static weak pointers to the VBOs, and the weak pointers will
/// expire immediately when you call this function. If you call those
/// functions again, they'll detect the expiration and request new buffers.</li>
/// </ul>
void Functions::cleanup()
{
    if (LOG_VBO_ALLOCATIONS) {
        qInfo() << "Cleanup";
    }

    getShaderPrograms().resetAll();
    getStaticVbos().resetAll();
    getTexLookup().clear();
}

ShaderPrograms &Functions::getShaderPrograms()
{
    return deref(m_shaderPrograms);
}
StaticVbos &Functions::getStaticVbos()
{
    return deref(m_staticVbos);
}

TexLookup &Functions::getTexLookup()
{
    return deref(m_texLookup);
}

std::shared_ptr<Functions> Functions::alloc()
{
    return std::make_shared<Functions>(Badge<Functions>{});
}

/// This only exists so we can detect errors in contexts that don't support \c glDebugMessageCallback().
void Functions::checkError()
{
#define CASE(x) \
    case (x): \
        qCritical() << "OpenGL error" << #x; \
        break;

    bool fail = false;
    while (true) {
        const auto err = Base::glGetError();
        if (err == GL_NO_ERROR) {
            break;
        }

        fail = true;

        switch (err) {
            CASE(GL_INVALID_ENUM)
            CASE(GL_INVALID_VALUE)
            CASE(GL_INVALID_OPERATION)
            CASE(GL_OUT_OF_MEMORY)
        default:
            qCritical() << "OpenGL error" << err;
            break;
        }
    }

    if (fail) {
        std::abort();
    }

#undef CASE
}

void Functions::applyBlendMode(BlendModeEnum mode)
{
    bool shouldBeEnabled = (mode != BlendModeEnum::NONE);
    if (shouldBeEnabled != m_blendEnabled) {
        if (shouldBeEnabled) {
            Base::glEnable(GL_BLEND);
        } else {
            Base::glDisable(GL_BLEND);
        }
        m_blendEnabled = shouldBeEnabled;
    }

    if (m_blendEnabled && mode != m_currentBlendMode) {
        switch (mode) {
        case BlendModeEnum::TRANSPARENCY:
            Base::glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
            break;
        case BlendModeEnum::ADDITIVE:
            Base::glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE, GL_ONE, GL_ONE);
            break;
        case BlendModeEnum::MODULATE:
             Base::glBlendFuncSeparate(GL_DST_COLOR, GL_ZERO, GL_DST_ALPHA, GL_ZERO);
            break;
        case BlendModeEnum::NONE:
             // This case should ideally not be reached if m_blendEnabled is true,
             // as m_blendEnabled would have been set to false and blending disabled.
             // If it is reached, it implies an attempt to set blend function while blending is conceptually off.
             // For safety, one might disable blending again or assert.
             assert(!m_blendEnabled && "BlendModeEnum::NONE with m_blendEnabled == true");
            break;
        default:
            assert(false && "Unknown BlendModeEnum");
            break;
        }
        m_currentBlendMode = mode;
    } else if (!m_blendEnabled) {
        // If blending is disabled, ensure our tracked blend mode reflects that.
        m_currentBlendMode = BlendModeEnum::NONE;
    }
}

void Functions::applyDepthState(const GLRenderState::OptDepth& depthFuncOpt)
{
    bool shouldBeEnabled = depthFuncOpt.has_value();
    if (shouldBeEnabled != m_depthTestEnabled) {
        if (shouldBeEnabled) {
            Base::glEnable(GL_DEPTH_TEST);
        } else {
            Base::glDisable(GL_DEPTH_TEST);
        }
        m_depthTestEnabled = shouldBeEnabled;
    }

    if (m_depthTestEnabled && depthFuncOpt != m_currentDepthFunction) {
        // A value must be present if m_depthTestEnabled is true from the block above
        assert(depthFuncOpt.has_value());
        switch (depthFuncOpt.value()) {
        case DepthFunctionEnum::NEVER: Base::glDepthFunc(GL_NEVER); break;
        case DepthFunctionEnum::LESS: Base::glDepthFunc(GL_LESS); break;
        case DepthFunctionEnum::EQUAL: Base::glDepthFunc(GL_EQUAL); break;
        case DepthFunctionEnum::LEQUAL: Base::glDepthFunc(GL_LEQUAL); break;
        case DepthFunctionEnum::GREATER: Base::glDepthFunc(GL_GREATER); break;
        case DepthFunctionEnum::NOTEQUAL: Base::glDepthFunc(GL_NOTEQUAL); break;
        case DepthFunctionEnum::GEQUAL: Base::glDepthFunc(GL_GEQUAL); break;
        case DepthFunctionEnum::ALWAYS: Base::glDepthFunc(GL_ALWAYS); break;
        default: assert(false && "Unknown DepthFunctionEnum"); break;
        }
        m_currentDepthFunction = depthFuncOpt;
    } else if (!m_depthTestEnabled) {
        // If depth test is disabled, ensure our tracked depth function reflects that.
        m_currentDepthFunction = std::nullopt;
    }
}

void Functions::applyShaderProgram(GLuint programId)
{
    if (programId != m_currentShaderProgramId) {
        Base::glUseProgram(programId);
        m_currentShaderProgramId = programId;
    }
}

void Functions::applyTexture(GLuint textureUnit, MMTextureId textureId, GLenum target)
{
    // Assuming m_currentTextureIds and m_currentTextureTargets are Array<..., 2>
    assert(textureUnit < 2 && "Texture unit out of bounds for current implementation");
    if (textureId != m_currentTextureIds[textureUnit] || target != m_currentTextureTargets[textureUnit]) {
        Base::glActiveTexture(GL_TEXTURE0 + textureUnit);
        Base::glBindTexture(target, textureId.asGLuint());
        m_currentTextureIds[textureUnit] = textureId;
        m_currentTextureTargets[textureUnit] = target;
    }
}

void Functions::applyLineParams(const LineParams& params)
{
    // For glLineWidth, only call if the value actually changes.
    // Use a small epsilon for floating-point comparison.
    if (std::abs(params.lineWidth - m_currentLineParams.lineWidth) > 0.001f) {
        Base::glLineWidth(params.lineWidth); // Directly use the base QOpenGLFunctions call
        m_currentLineParams.lineWidth = params.lineWidth;
    }
    // Note: The glLineWidth method in this class was commented out or had a scaling factor.
    // Calling Base::glLineWidth directly is more straightforward for state management.
    // If stippling or other line parameters were supported, they would be handled here.
}

void Functions::applyPointSize(const std::optional<float>& pointSize)
{
    bool newHasValue = pointSize.has_value();
    bool currentHasValue = m_currentPointSize.has_value();

    if (newHasValue != currentHasValue) {
        enableProgramPointSize(newHasValue); // Manages GL_PROGRAM_POINT_SIZE or similar
    }

    // If fixed-function point size is used (i.e., enableProgramPointSize(false) was called),
    // and the point size value changes, then call glPointSize.
    // This part depends on how enableProgramPointSize behaves.
    // If enableProgramPointSize(true) means gl_PointSize is used in shader, then CPU-side glPointSize is irrelevant.
    // For now, just track the conceptual state. The actual setting of size might be shader-side.
    m_currentPointSize = pointSize;
    // If direct glPointSize control was intended when enableProgramPointSize(false):
    // if (newHasValue && !enableProgramPointSizeTrueImpliesShaderControl) { // Fictional flag
    //     if (!currentHasValue || std::abs(pointSize.value() - m_currentPointSize.value()) > 0.001f) {
    //         Base::glPointSize(pointSize.value());
    //     }
    // }
}

void Functions::applyCulling(CullingEnum cullMode)
{
    bool shouldBeEnabled = (cullMode != CullingEnum::NONE);
    if (shouldBeEnabled != m_cullingEnabled) {
        if (shouldBeEnabled) {
            Base::glEnable(GL_CULL_FACE);
        } else {
            Base::glDisable(GL_CULL_FACE);
        }
        m_cullingEnabled = shouldBeEnabled;
    }

    if (m_cullingEnabled && cullMode != m_currentCullingMode) {
        switch (cullMode) {
        // Case CullingEnum::NONE is implicitly handled if m_cullingEnabled is true
        // but cullMode is NONE, which is a contradiction handled by the assert.
        case CullingEnum::FRONT:
            Base::glCullFace(GL_FRONT);
            break;
        case CullingEnum::BACK:
            Base::glCullFace(GL_BACK);
            break;
        case CullingEnum::FRONT_AND_BACK:
            Base::glCullFace(GL_FRONT_AND_BACK);
            break;
        default:
             assert(cullMode != CullingEnum::NONE && "CullingEnum::NONE passed while m_cullingEnabled is true");
             assert(false && "Unknown CullingEnum or invalid state");
            break;
        }
        m_currentCullingMode = cullMode;
    } else if (!m_cullingEnabled) {
        // If culling is disabled, ensure our tracked cull mode reflects that.
        m_currentCullingMode = CullingEnum::NONE;
    }
}

} // namespace Legacy
