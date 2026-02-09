#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "../global/Array.h"
#include "../global/Color.h"
#include "../global/ConfigConsts.h"
#include "../global/IndexedVector.h"
#include "../global/NamedColors.h"
#include "../global/TaggedInt.h"
#include "../global/hash.h"
#include "../global/utils.h"
#include "FontFormatFlags.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>

#include <glm/glm.hpp>

#include <QDebug>
#include <QOpenGLTexture>
#include <qopengl.h>

struct NODISCARD TexVert final
{
    glm::vec3 tex{};
    glm::vec3 vert{};

    explicit TexVert(const glm::vec3 &tex_, const glm::vec3 &vert_)
        : tex{tex_}
        , vert{vert_}
    {}
};

using TexVertVector = std::vector<TexVert>;

struct NODISCARD ColoredTexVert final
{
    Color color;
    glm::vec3 tex{};
    glm::vec3 vert{};

    explicit ColoredTexVert(const Color color_, const glm::vec3 &tex_, const glm::vec3 &vert_)
        : color{color_}
        , tex{tex_}
        , vert{vert_}
    {}
};

struct NODISCARD RoomQuadTexVert final
{
    // xyz = room coord, w = (colorId << 8 | tex_z)
    // - colorId: packed into bits 8-15 (must be < MAX_NAMED_COLORS)
    // - tex_z:   packed into bits 0-7  (must be < 256)
    glm::ivec4 vertTexCol{};

    explicit RoomQuadTexVert(const glm::ivec3 &vert, const int tex_z)
        : RoomQuadTexVert{vert, tex_z, NamedColorEnum::DEFAULT}
    {}

    explicit RoomQuadTexVert(const glm::ivec3 &vert, const int tex_z, const NamedColorEnum color)
        : vertTexCol{vert, (static_cast<uint8_t>(color) << 8) | tex_z}
    {
        if (IS_DEBUG_BUILD) {
            constexpr auto max_texture_layers = 256;
            const auto c = static_cast<uint8_t>(color);
            assert(c == std::clamp<uint8_t>(c, 0, NUM_NAMED_COLORS - 1));
            assert(tex_z == std::clamp(tex_z, 0, max_texture_layers - 1));
        }
    }
};

using ColoredTexVertVector = std::vector<ColoredTexVert>;

struct NODISCARD ColorVert final
{
    Color color;
    glm::vec3 vert{};

    explicit ColorVert(const Color color_, const glm::vec3 &vert_)
        : color{color_}
        , vert{vert_}
    {}
};

struct NODISCARD GlyphMetrics final
{
    // xy: uvOffset (0.0 to 1.0)
    // zw: uvSize (0.0 to 1.0)
    glm::vec4 uvRect{};

    // xy: pixelOffset relative to cursor baseline (pre-scaled by DPR during font loading)
    // zw: pixelSize (pre-scaled by DPR during font loading)
    // Note: Synthetic glyphs (background/underline) typically ignore this in favor of VBO data.
    glm::vec4 posRect{};
};
static_assert(sizeof(GlyphMetrics) == 32);

struct NODISCARD IconMetrics final
{
    enum Flags : uint32_t {
        SCREEN_SPACE = 1 << 0,
        FIXED_SIZE = 1 << 1,
        DISTANCE_SCALE = 1 << 2,
        CLAMP_TO_EDGE = 1 << 3,
        AUTO_ROTATE = 1 << 4,
    };

    // xy: Default size in world units or pixels (used if instance size is zero)
    // zw: Relative anchor offset (-0.5 is centered, 0.0 is top-left)
    glm::vec4 sizeAnchor{};
    uint32_t flags = 0;
    uint32_t padding[3]{};
};
static_assert(sizeof(IconMetrics) == 32);

// Instance data for font rendering.
// the font's vertex shader transforms the world position to screen space,
// rounds to integer pixel offset, and then adds the (possibly rotated/italicized)
// vertex offset in screen space.
struct NODISCARD FontInstanceData final
{
    glm::vec3 base{};     // 12 bytes: world space
    uint32_t color = 0;   // 4 bytes: RGBA color (or instance alpha for named colors)
    int16_t offsetX = 0;  // 2 bytes: cursorX for glyphs, or rect.x for synthetic
    uint16_t packed1 = 0; // 2 bytes: glyphId (10), flags (6)
    uint32_t packedRest
        = 0; // 4 bytes: rotation/namedColorIndex OR offsetY/sizeW/sizeH/namedColorIndex

    explicit FontInstanceData(const glm::vec3 &base_,
                              uint32_t color_,
                              int16_t offsetX_,
                              int16_t offsetY_,
                              int16_t sizeW_,
                              int16_t sizeH_,
                              uint16_t glyphId_,
                              int16_t rotation_,
                              uint8_t flags_,
                              uint8_t namedColorIndex_)
        : base{base_}
        , color{color_}
        , offsetX{offsetX_}
        , packed1{static_cast<uint16_t>((static_cast<uint32_t>(glyphId_) & 0x3FFu)
                                       | ((static_cast<uint32_t>(flags_) & 0x3Fu) << 10))}
    {
        const uint32_t uNamedColorIndex = static_cast<uint32_t>(namedColorIndex_) & 0x3Fu;

        if (glyphId_ < 256) {
            // Regular character: pack rotation (9), namedColorIndex (6)
            const uint32_t uRotation = static_cast<uint32_t>(static_cast<uint16_t>(rotation_))
                                       & 0x1FFu;
            packedRest = uRotation | (uNamedColorIndex << 9);
        } else {
            // Synthetic glyph: pack offsetY (8), sizeW (10), sizeH (8), namedColorIndex (6)
            const uint32_t uOffsetY = static_cast<uint32_t>(static_cast<int8_t>(offsetY_)) & 0xFFu;
            const uint32_t uSizeW = static_cast<uint32_t>(static_cast<uint16_t>(sizeW_)) & 0x3FFu;
            const uint32_t uSizeH = static_cast<uint32_t>(static_cast<int8_t>(sizeH_)) & 0xFFu;

            packedRest = uOffsetY | (uSizeW << 8) | (uSizeH << 18) | (uNamedColorIndex << 26);
        }
    }
};
static_assert(sizeof(FontInstanceData) == 24);

// Instance data for icons (characters, arrows, selections).
struct NODISCARD IconInstanceData final
{
    glm::vec3 base{};   // world space or screen pixels
    uint32_t color = 0; // 4 bytes: RGBA color
    int16_t sizeW = 0, sizeH = 0;
    uint32_t packed = 0; // rotation (9), iconIndex (8)

    explicit IconInstanceData(const glm::vec3 &base_,
                              uint32_t color_,
                              int16_t sizeW_,
                              int16_t sizeH_,
                              uint16_t iconIndex,
                              int16_t rotation)
        : base{base_}
        , color{color_}
        , sizeW{sizeW_}
        , sizeH{sizeH_}
    {
        const uint32_t uRotation = static_cast<uint32_t>(static_cast<uint16_t>(rotation)) & 0x1FFu;
        const uint32_t uIconIndex = static_cast<uint32_t>(iconIndex) & 0xFFu;

        packed = uRotation | (uIconIndex << 9);
    }
};
static_assert(sizeof(IconInstanceData) == 24);

enum class NODISCARD DrawModeEnum {
    INVALID = 0,
    POINTS = 1,
    LINES = 2,
    TRIANGLES = 3,
    QUADS = 4,
    INSTANCED_QUADS = 5
};

struct NODISCARD LineParams final
{
    float width = 1.f;
    LineParams() = default;

    explicit LineParams(const float width_)
        : width{width_}
    {}
};

#define XFOREACH_DEPTHFUNC(X) \
    X(NEVER) \
    X(LESS) \
    X(EQUAL) \
    X(LEQUAL) \
    X(GREATER) \
    X(NOTEQUAL) \
    X(GEQUAL) \
    X(ALWAYS)

#define X_DECL(X) X = GL_##X,
enum class NODISCARD DepthFunctionEnum { XFOREACH_DEPTHFUNC(X_DECL) DEFAULT = LESS };
#undef X_DECL

enum class NODISCARD BlendModeEnum {
    /* glDisable(GL_BLEND); */
    NONE,
    /* This is the MMapper2 default setting, but not OpenGL default setting
     * glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); */
    TRANSPARENCY,
    /* This mode allows you to multiply by the painted color, in the range [0,1].
     * glEnable(GL_BLEND); glBlendFuncSeparate(GL_ZERO, GL_SRC_COLOR, GL_ZERO, GL_ONE); */
    MODULATE,
};

enum class NODISCARD CullingEnum {

    // Culling is disabled: glDisable(GL_CULL_FACE)
    DISABLED,

    // GL_BACK: back faces are culled (the usual default if GL_CULL_FACE is enabled)
    BACK,

    // GL_FRONT: front faces are culled
    FRONT,

    // GL_FRONT_AND_BACK: both front and back faces are culled
    // (you probably don't ever want this)
    FRONT_AND_BACK

};

namespace tags {
struct NODISCARD MMTextureIdTag final
{};
}; // namespace tags

struct NODISCARD MMTextureId final : TaggedInt<MMTextureId, tags::MMTextureIdTag, int32_t, -1>
{
    using TaggedInt::TaggedInt;
    NODISCARD explicit operator bool() { return value() > DEFAULT_VALUE; }
    NODISCARD explicit operator size_t() const
    {
        if (value() < 0) {
            throw std::runtime_error("negative index");
        }
        return static_cast<size_t>(value());
    }
};
static constexpr const MMTextureId INVALID_MM_TEXTURE_ID{MMTextureId::DEFAULT_VALUE};

template<>
struct std::hash<MMTextureId>
{
    std::size_t operator()(const MMTextureId id) const noexcept { return numeric_hash(id.value()); }
};

class MMTexture;
using SharedMMTexture = std::shared_ptr<MMTexture>;
using TexLookup = IndexedVector<SharedMMTexture, MMTextureId>;
using SharedTexLookup = std::shared_ptr<TexLookup>;

struct NODISCARD GLRenderState final
{
    // glEnable(GL_BLEND)
    BlendModeEnum blend = BlendModeEnum::NONE;

    CullingEnum culling = CullingEnum::DISABLED;

    // glEnable(GL_DEPTH_TEST) + glDepthFunc()
    using OptDepth = std::optional<DepthFunctionEnum>;
    OptDepth depth;

    // glLineWidth() + { glEnable(LINE_STIPPLE) + glLineStipple() }
    LineParams lineParams;

    using Textures = MMapper::Array<MMTextureId, 2>;
    struct NODISCARD Uniforms final
    {
        Color color;
        // glEnable(TEXTURE_2D), or glEnable(TEXTURE_3D)
        Textures textures;
        std::optional<float> pointSize;
        float dprScale = 1.0f;
    };

    Uniforms uniforms;

    NODISCARD GLRenderState withBlend(const BlendModeEnum new_blend) const
    {
        GLRenderState copy = *this;
        copy.blend = new_blend;
        return copy;
    }

    NODISCARD GLRenderState withColor(const Color new_color) const
    {
        GLRenderState copy = *this;
        copy.uniforms.color = new_color;
        return copy;
    }

    NODISCARD GLRenderState withCulling(const CullingEnum new_culling) const
    {
        GLRenderState copy = *this;
        copy.culling = new_culling;
        return copy;
    }

    NODISCARD GLRenderState withDepthFunction(const DepthFunctionEnum new_depth) const
    {
        GLRenderState copy = *this;
        copy.depth = new_depth;
        return copy;
    }

    NODISCARD GLRenderState withDepthFunction(std::nullopt_t) const
    {
        GLRenderState copy = *this;
        copy.depth.reset();
        return copy;
    }

    NODISCARD GLRenderState withLineParams(const LineParams &new_lineParams) const
    {
        GLRenderState copy = *this;
        copy.lineParams = new_lineParams;
        return copy;
    }

    NODISCARD GLRenderState withPointSize(const GLfloat size) const
    {
        GLRenderState copy = *this;
        copy.uniforms.pointSize = size;
        return copy;
    }

    NODISCARD GLRenderState withDprScale(const float scale) const
    {
        GLRenderState copy = *this;
        copy.uniforms.dprScale = scale;
        return copy;
    }

    NODISCARD GLRenderState withTexture0(const MMTextureId new_texture) const
    {
        GLRenderState copy = *this;
        copy.uniforms.textures = Textures{new_texture, INVALID_MM_TEXTURE_ID};
        return copy;
    }
};

struct NODISCARD IRenderable
{
public:
    IRenderable() = default;
    virtual ~IRenderable();

public:
    DELETE_CTORS_AND_ASSIGN_OPS(IRenderable);

private:
    // Clears the contents of the mesh, but does not give up its GL resources.
    virtual void virt_clear() = 0;
    // Clears the mesh and destroys the GL resources.
    virtual void virt_reset() = 0;
    NODISCARD virtual bool virt_isEmpty() const = 0;

private:
    NODISCARD virtual bool virt_modifiesRenderState() const { return false; }
    NODISCARD virtual GLRenderState virt_modifyRenderState(const GLRenderState &input) const
    {
        assert(false);
        return input;
    }
    virtual void virt_render(const GLRenderState &renderState) = 0;

public:
    // Clears the contents of the mesh, but does not give up its GL resources.
    void clear() { virt_clear(); }
    // Clears the mesh and destroys the GL resources.
    void reset() { virt_reset(); }
    NODISCARD bool isEmpty() const { return virt_isEmpty(); }

public:
    void render(const GLRenderState &renderState)
    {
        if (!virt_modifiesRenderState()) {
            virt_render(renderState);
            return;
        }
        const GLRenderState modifiedState = virt_modifyRenderState(renderState);
        virt_render(modifiedState);
    }
};

struct NODISCARD TexturedRenderable final : public IRenderable
{
private:
    MMTextureId m_texture;
    std::unique_ptr<IRenderable> m_mesh;

public:
    explicit TexturedRenderable(MMTextureId tex, std::unique_ptr<IRenderable> mesh);
    ~TexturedRenderable() final;

public:
    DELETE_CTORS_AND_ASSIGN_OPS(TexturedRenderable);

private:
    void virt_clear() final;
    void virt_reset() final;
    NODISCARD bool virt_isEmpty() const final;

public:
    void virt_render(const GLRenderState &renderState) final;

public:
    NODISCARD MMTextureId replaceTexture(const MMTextureId tex)
    {
        return std::exchange(m_texture, tex);
    }
};

enum class NODISCARD BufferUsageEnum { STATIC_DRAW, DYNAMIC_DRAW };

class NODISCARD UniqueMesh final
{
private:
    std::unique_ptr<IRenderable> m_mesh;

public:
    UniqueMesh() = default;
    explicit UniqueMesh(std::unique_ptr<IRenderable> mesh)
        : m_mesh{std::move(mesh)}
    {
        std::ignore = deref(m_mesh);
    }
    ~UniqueMesh() = default;
    DEFAULT_MOVES_DELETE_COPIES(UniqueMesh);

    void render(const GLRenderState &rs) const { deref(m_mesh).render(rs); }
    NODISCARD explicit operator bool() const { return m_mesh != nullptr; }
};

struct NODISCARD UniqueMeshVector final
{
private:
    std::vector<UniqueMesh> m_meshes;

public:
    UniqueMeshVector() = default;
    explicit UniqueMeshVector(std::vector<UniqueMesh> &&meshes)
        : m_meshes{std::move(meshes)}
    {}

    void render(const GLRenderState &rs)
    {
        for (auto &mesh : m_meshes) {
            mesh.render(rs);
        }
    }
};

struct NODISCARD Viewport final
{
    glm::ivec2 offset{};
    glm::ivec2 size{};
};

static constexpr const size_t VERTS_PER_LINE = 2;
static constexpr const size_t VERTS_PER_TRI = 3;
static constexpr const size_t VERTS_PER_QUAD = 4;
