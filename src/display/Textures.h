#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "../global/Badge.h"
#include "../global/EnumIndexedArray.h"
#include "../map/mmapper2room.h"
#include "../opengl/OpenGLTypes.h"
#include "RoadIndex.h"

#include <functional>
#include <memory>

#include <QOpenGLTexture>
#include <QString>
#include <QtGui/qopengl.h>

constexpr int ICON_WIDTH = 32;
constexpr int ICON_HEIGHT = 32;

// currently forward declared in OpenGLTypes.h
// so it can define SharedMMTexture
class NODISCARD MMTexture final : public std::enable_shared_from_this<MMTexture>
{
private:
    QOpenGLTexture m_qt_texture;
    MMTextureId m_id = INVALID_MM_TEXTURE_ID;
    bool m_forbidUpdates = false;

    // For adopted textures
    bool m_is_adopted = false;
    GLuint m_adopted_texture_id = 0;
    QOpenGLTexture::Target m_adopted_target = QOpenGLTexture::Target2D; // Default to a valid target

public:
    NODISCARD static std::shared_ptr<MMTexture> alloc(const QString &name)
    {
        return std::make_shared<MMTexture>(Badge<MMTexture>{}, name);
    }
    NODISCARD static std::shared_ptr<MMTexture> alloc(
        const QOpenGLTexture::Target target,
        const std::function<void(QOpenGLTexture &)> &init,
        const bool forbidUpdates)
    {
        return std::make_shared<MMTexture>(Badge<MMTexture>{}, target, init, forbidUpdates);
    }

    NODISCARD static std::shared_ptr<MMTexture> allocAdopted(GLuint adopted_gl_id, QOpenGLTexture::Target adopted_target, bool forbidUpdates = true)
    {
        return std::make_shared<MMTexture>(Badge<MMTexture>{}, adopted_gl_id, adopted_target, forbidUpdates);
    }

public:
    MMTexture() = delete;
    MMTexture(Badge<MMTexture>, const QString &name);
    MMTexture(Badge<MMTexture>,
              const QOpenGLTexture::Target target,
              const std::function<void(QOpenGLTexture &)> &init,
              const bool forbidUpdates)
        : m_qt_texture{target}
        , m_forbidUpdates{forbidUpdates}
        , m_is_adopted{false} // Ensure this is false for normally constructed textures
    {
        init(m_qt_texture);
    }
    // Constructor for adopted textures
    MMTexture(Badge<MMTexture>, GLuint adopted_gl_id, QOpenGLTexture::Target adopted_target_enum, bool forbidUpdates_flag)
        : m_qt_texture(QOpenGLTexture::Target2D) // Initialize internal QOpenGLTexture with a valid, minimal target
        , m_forbidUpdates{forbidUpdates_flag}
        , m_is_adopted{true}
        , m_adopted_texture_id{adopted_gl_id}
        , m_adopted_target{adopted_target_enum} // Store the actual adopted target
    {
        // Do not call create() or allocateStorage() on m_qt_texture for adopted textures.
        // It's just a shell or placeholder if m_is_adopted is true.
    }
    DELETE_CTORS_AND_ASSIGN_OPS(MMTexture);

public:
    NODISCARD QOpenGLTexture *get() { return m_is_adopted ? nullptr : &m_qt_texture; } // Or handle differently if direct access to QOpenGLTexture is needed for adopted
    NODISCARD const QOpenGLTexture *get() const { return m_is_adopted ? nullptr : &m_qt_texture; }
    // operator-> might be problematic if get() can return nullptr. Consider implications or disallow for adopted.
    QOpenGLTexture *operator->() { return get(); }


    // bind() will likely need adjustment if used with adopted textures, or rendering path must handle adopted textures specially.
    void bind() { if (!m_is_adopted) get()->bind(); else { /* TODO: How to bind adopted texture? Or ensure this is not called. */ } }
    void bind(GLuint x) { if (!m_is_adopted) get()->bind(x); else { /* TODO: How to bind adopted texture? Or ensure this is not called. */ } }
    void release(GLuint x) { if (!m_is_adopted) get()->release(x); else { /* TODO: How to release adopted texture? Or ensure this is not called. */ } }

    NODISCARD GLuint textureId() const { return m_is_adopted ? m_adopted_texture_id : m_qt_texture.textureId(); }
    NODISCARD QOpenGLTexture::Target target() const { return m_is_adopted ? m_adopted_target : m_qt_texture.target(); }
    NODISCARD bool canBeUpdated() const { return m_is_adopted ? !m_forbidUpdates : !m_forbidUpdates && m_qt_texture.isCreated(); } // Adopted might be updatable if not forbidUpdates
    NODISCARD bool isAdopted() const { return m_is_adopted; }

    NODISCARD SharedMMTexture getShared() { return shared_from_this(); }
    NODISCARD MMTexture *getRaw() { return this; }

    NODISCARD MMTextureId getId() const
    {
        assert(m_id != INVALID_MM_TEXTURE_ID);
        return m_id;
    }

    // only called by MapCanvas::initTextures() and GLFont::init();
    // don't forget to call OpenGL::setTextureLookup(), too.
    void setId(const MMTextureId id)
    {
        assert(m_id == INVALID_MM_TEXTURE_ID);
        m_id = id;
    }

    void clearId()
    {
        assert(m_id != INVALID_MM_TEXTURE_ID);
        m_id = INVALID_MM_TEXTURE_ID;
    }
};

template<typename E>
using texture_array = EnumIndexedArray<SharedMMTexture, E>;

template<RoadTagEnum Tag>
struct NODISCARD road_texture_array : private texture_array<RoadIndexMaskEnum>
{
    using base = texture_array<RoadIndexMaskEnum>;
    decltype(auto) operator[](TaggedRoadIndex<Tag> x) { return base::operator[](x.index); }
    using base::operator[];
    using base::begin;
    using base::end;
    using base::for_each;
    using base::size;
};

using TextureArrayNESW = EnumIndexedArray<SharedMMTexture, ExitDirEnum, NUM_EXITS_NESW>;
using TextureArrayNESWUD = EnumIndexedArray<SharedMMTexture, ExitDirEnum, NUM_EXITS_NESWUD>;

// X(_Type, _Name)
#define XFOREACH_MAPCANVAS_TEXTURES(X) \
    X(texture_array<RoomTerrainEnum>, terrain) \
    X(road_texture_array<RoadTagEnum::ROAD>, road) \
    X(road_texture_array<RoadTagEnum::TRAIL>, trail) \
    X(texture_array<RoomMobFlagEnum>, mob) \
    X(texture_array<RoomLoadFlagEnum>, load) \
    X(TextureArrayNESW, wall) \
    X(TextureArrayNESW, dotted_wall) \
    X(TextureArrayNESWUD, stream_in) \
    X(TextureArrayNESWUD, stream_out) \
    X(TextureArrayNESWUD, door) \
    X(SharedMMTexture, char_arrows) \
    X(SharedMMTexture, char_room_sel) \
    X(SharedMMTexture, exit_climb_down) \
    X(SharedMMTexture, exit_climb_up) \
    X(SharedMMTexture, exit_down) \
    X(SharedMMTexture, exit_up) \
    X(SharedMMTexture, no_ride) \
    X(SharedMMTexture, room_sel) \
    X(SharedMMTexture, room_sel_distant) \
    X(SharedMMTexture, room_sel_move_bad) \
    X(SharedMMTexture, room_sel_move_good) \
    X(SharedMMTexture, room_needs_update) \
    X(SharedMMTexture, room_modified)

struct NODISCARD MapCanvasTextures final
{
#define X_DECL(_Type, _Name) _Type _Name;
    XFOREACH_MAPCANVAS_TEXTURES(X_DECL)
#undef X_DECL

    SharedMMTexture icon_texture_array; // For GL_TEXTURE_2D_ARRAY
    std::map<RoomMobFlagEnum, int> mob_icon_layers;
    std::map<RoomLoadFlagEnum, int> load_icon_layers;
    std::optional<int> no_ride_icon_layer;
    // TODO: Add other icon layers as needed e.g. room_sel_distant, room_needs_update, room_modified etc.

    // Map from original individual icon texture ID to its layer in icon_texture_array
    std::map<MMTextureId, int> individual_texture_to_array_layer;

private:
    template<typename Callback>
    static void apply_callback(SharedMMTexture &tex, Callback &&callback)
    {
        callback(tex);
    }

    template<typename T, typename Callback>
    static void apply_callback(T &x, Callback &&callback)
    {
        x.for_each(callback);
    }

public:
    template<typename Callback>
    void for_each(Callback &&callback)
    {
#define X_EACH(_Type, _Name) apply_callback(_Name, callback);
        XFOREACH_MAPCANVAS_TEXTURES(X_EACH)
#undef X_EACH
        // Manually handle the new members if they need to be part of for_each
        if (icon_texture_array) callback(icon_texture_array);
        // The maps (mob_icon_layers, etc.) don't store SharedMMTexture, so they are not part of this loop.
    }

    void destroyAll();
};

namespace mctp {

namespace detail {
template<typename E_, size_t Size_>
auto typeHack(EnumIndexedArray<SharedMMTexture, E_, Size_>)
    -> EnumIndexedArray<MMTextureId, E_, Size_>;

template<typename T>
struct NODISCARD Proxy
{
    using type = decltype(typeHack(std::declval<T>()));
};

template<>
struct NODISCARD Proxy<SharedMMTexture>
{
    using type = MMTextureId;
};

template<typename T>
using proxy_t = typename Proxy<T>::type;
} // namespace detail

struct NODISCARD MapCanvasTexturesProxy final
{
#define X_DECL_PROXY(_Type, _Name) mctp::detail::proxy_t<_Type> _Name;
    XFOREACH_MAPCANVAS_TEXTURES(X_DECL_PROXY)
#undef X_DECL_PROXY
};

NODISCARD extern MapCanvasTexturesProxy getProxy(const MapCanvasTextures &mct);

} // namespace mctp

NODISCARD extern MMTextureId allocateTextureId();
