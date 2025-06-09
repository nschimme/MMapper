// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "Textures.h"

#include "../configuration/configuration.h"
#include "../global/thread_utils.h"
#include "../global/utils.h"
#include "../opengl/Font.h"
#include "../opengl/OpenGLTypes.h"
#include "Filenames.h"
#include "RoadIndex.h"
#include "mapcanvas.h"

#include <optional>
#include <stdexcept>
#include <tuple>
#include <vector>

#include <glm/glm.hpp>

#include <QMessageLogContext>
#include <QtCore>
#include <QtGui>

MMTextureId allocateTextureId()
{
    ABORT_IF_NOT_ON_MAIN_THREAD();
    static MMTextureId next{0};
    return next++;
}

MMTexture::MMTexture(Badge<MMTexture>, const QString &name)
    : m_qt_texture{QImage{name}.mirrored()}
{
    auto &tex = m_qt_texture;
    tex.setWrapMode(QOpenGLTexture::WrapMode::MirroredRepeat);
    tex.setMinMagFilters(QOpenGLTexture::Filter::LinearMipMapLinear, QOpenGLTexture::Filter::Linear);
}

void MapCanvasTextures::destroyAll()
{
    for_each([](SharedMMTexture &tex) -> void { tex.reset(); });
}

NODISCARD static SharedMMTexture loadTexture(const QString &name)
{
    auto mmtex = MMTexture::alloc(name);
    auto *texture = mmtex->get();
    if (!texture->isCreated()) {
        qWarning() << "failed to create: " << name;
        texture->setSize(1);
        texture->create();

        if (!texture->isCreated()) {
            throw std::runtime_error(mmqt::toStdStringUtf8("failed to create: " + name));
        }
    }

    texture->setWrapMode(QOpenGLTexture::WrapMode::MirroredRepeat);
    return mmtex;
}

template<typename E>
static void loadPixmapArray(texture_array<E> &textures)
{
    const auto N = textures.size();
    for (size_t i = 0u; i < N; ++i) {
        const auto x = static_cast<E>(i);
        textures[x] = loadTexture(getPixmapFilename(x));
    }
}

template<RoadTagEnum Tag>
static void loadPixmapArray(road_texture_array<Tag> &textures)
{
    const auto N = textures.size();
    for (size_t i = 0u; i < N; ++i) {
        const auto x = TaggedRoadIndex<Tag>{static_cast<RoadIndexMaskEnum>(i)};
        textures[x] = loadTexture(getPixmapFilename(x));
    }
}

// Technically only the "minifying" filter can be trilinear.
//
// GL_NEAREST = 1 sample from level 0 (no mipmapping).
// GL_LINEAR = 4 samples from level 0 (no mipmapping).
//
// GL_NEAREST_MIPMAP_NEAREST = 1 sample (nearest mip).
// GL_NEAREST_MIPMAP_LINEAR = 2 samples (samples 2 nearest mips).
//
// GL_LINEAR_MIPMAP_NEAREST = 4 samples (nearest mip).
// GL_LINEAR_MIPMAP_LINEAR = 8 samples (trilinear).
//
static void setTrilinear(const SharedMMTexture &mmtex, const bool trilinear)
{
    if (mmtex == nullptr) {
        return;
    }
    if (QOpenGLTexture *const qtex = mmtex->get()) {
        qtex->setMinMagFilters(
            /* "minifying" filter */
            trilinear ? QOpenGLTexture::LinearMipMapLinear /* 8 samples */
                      : QOpenGLTexture::NearestMipMapLinear /* 2 samples (default) */,
            /* magnifying filter */
            QOpenGLTexture::Linear /* 4 samples (default) */);
    }
}

NODISCARD static SharedMMTexture createDottedWall(const ExitDirEnum dir)
{
    static constexpr const uint32_t MAX_BITS = 7;
    static constexpr const int SIZE = 1 << MAX_BITS;

    const auto init = [dir](QOpenGLTexture &tex) -> void {
        const QColor OPAQUE_WHITE = Qt::white;
        const QColor TRANSPARENT_BLACK = QColor::fromRgbF(0.0, 0.0, 0.0, 0.0);
        MMapper::Array<QImage, MAX_BITS + 1> images;

        for (auto i = 0u; i <= MAX_BITS; ++i) {
            const int size = 1 << (MAX_BITS - i);
            QImage image{size, size, QImage::Format::Format_RGBA8888};
            image.fill(TRANSPARENT_BLACK);
            if (size >= 16) {
                // 64 and 128:
                // ##..##..##..##..##..##..##..##..##..##..##..##..##..##..##..##..
                // ##..##..##..##..##..##..##..##..##..##..##..##..##..##..##..##..
                // ##..##..##..##..##..##..##..##..##..##..##..##..##..##..##..##..
                // ##..##..##..##..##..##..##..##..##..##..##..##..##..##..##..##..
                // 32:
                // ##..##..##..##..##..##..##..##..
                // ##..##..##..##..##..##..##..##..
                // 16:
                // ##..##..##..##..

                const int width = [i]() -> int {
                    switch (MAX_BITS - i) {
                    case 4:
                        return 1;
                    case 5:
                        return 2;
                    case 6:
                    case 7:
                        return 4;
                    default:
                        assert(false);
                        return 4;
                    }
                }();

                assert(isClamped(width, 1, 4));

                for (int y = 0; y < width; ++y) {
                    for (int x = 0; x < size; x += 4) {
                        image.setPixelColor(x + 0, y, OPAQUE_WHITE);
                        image.setPixelColor(x + 1, y, OPAQUE_WHITE);
                    }
                }
            } else if (size == 8) {
                // #...#...
                image.setPixelColor(1, 0, OPAQUE_WHITE);
                image.setPixelColor(5, 0, OPAQUE_WHITE);
            } else if (size == 4) {
                // -.-.
                image.setPixelColor(0, 0, QColor::fromRgbF(1.0, 1.0, 1.0, 0.5));
                image.setPixelColor(2, 0, QColor::fromRgbF(1.0, 1.0, 1.0, 0.5));
            } else if (size == 2) {
                // ..
                image.setPixelColor(0, 0, QColor::fromRgbF(1.0, 1.0, 1.0, 0.25));
                image.setPixelColor(1, 0, QColor::fromRgbF(1.0, 1.0, 1.0, 0.25));
            }

            if (dir == ExitDirEnum::EAST || dir == ExitDirEnum::WEST) {
                const auto halfSize = static_cast<double>(size) * 0.5;
                QTransform matrix;
                matrix.translate(halfSize, halfSize);
                matrix.rotate(90);
                matrix.translate(-halfSize, -halfSize);
                images[i] = image.transformed(matrix, Qt::FastTransformation);
            } else {
                images[i] = image;
            }

            if (dir == ExitDirEnum::NORTH || dir == ExitDirEnum::WEST) {
                images[i] = images[i].mirrored(true, true);
            }
        }

        tex.setWrapMode(QOpenGLTexture::WrapMode::MirroredRepeat);
        tex.setMinMagFilters(QOpenGLTexture::Filter::NearestMipMapNearest,
                             QOpenGLTexture::Filter::Nearest);
        tex.setAutoMipMapGenerationEnabled(false);
        tex.create();
        tex.setSize(SIZE, SIZE, 1);
        tex.setMipLevels(tex.maximumMipLevels());
        tex.setFormat(QOpenGLTexture::TextureFormat::RGBA8_UNorm);
        tex.allocateStorage(QOpenGLTexture::PixelFormat::RGBA, QOpenGLTexture::PixelType::UInt8);

        tex.setData(QOpenGLTexture::PixelFormat::RGBA,
                    QOpenGLTexture::PixelType::UInt8,
                    images[0].constBits());
        for (auto i = 1u; i <= MAX_BITS; ++i) {
            tex.setData(static_cast<int>(i),
                        QOpenGLTexture::PixelFormat::RGBA,
                        QOpenGLTexture::PixelType::UInt8,
                        images[i].constBits());
        }
    };

    return MMTexture::alloc(
        QOpenGLTexture::Target::Target2D, [&init](QOpenGLTexture &tex) { return init(tex); }, true);
}

void MapCanvas::initTextures()
{
    MapCanvasTextures &textures = this->m_textures;
    loadPixmapArray(textures.terrain); // 128
    loadPixmapArray(textures.road);    // 128
    loadPixmapArray(textures.trail);   // 64
    loadPixmapArray(textures.mob);     // 128
    loadPixmapArray(textures.load);    // 128

    for (const ExitDirEnum dir : ALL_EXITS_NESW) {
        textures.dotted_wall[dir] = createDottedWall(dir);
        textures.wall[dir] = loadTexture(
            getPixmapFilenameRaw(QString::asprintf("wall-%s.png", lowercaseDirection(dir))));
    }
    for (const ExitDirEnum dir : ALL_EXITS_NESWUD) {
        // doors are 256
        textures.door[dir] = loadTexture(
            getPixmapFilenameRaw(QString::asprintf("door-%s.png", lowercaseDirection(dir))));
        // streams are 128
        textures.stream_in[dir] = loadTexture(
            getPixmapFilenameRaw(QString::asprintf("stream-in-%s.png", lowercaseDirection(dir))));
        textures.stream_out[dir] = loadTexture(
            getPixmapFilenameRaw(QString::asprintf("stream-out-%s.png", lowercaseDirection(dir))));
    }

    // char images are 256
    textures.char_arrows = loadTexture(getPixmapFilenameRaw("char-arrows.png"));
    textures.char_room_sel = loadTexture(getPixmapFilenameRaw("char-room-sel.png"));
    // exits are 128
    textures.exit_climb_down = loadTexture(getPixmapFilenameRaw("exit-climb-down.png"));
    textures.exit_climb_up = loadTexture(getPixmapFilenameRaw("exit-climb-up.png"));
    textures.exit_down = loadTexture(getPixmapFilenameRaw("exit-down.png"));
    textures.exit_up = loadTexture(getPixmapFilenameRaw("exit-up.png"));
    textures.no_ride = loadTexture(getPixmapFilenameRaw("no-ride.png"));
    // room are 256
    textures.room_sel = loadTexture(getPixmapFilenameRaw("room-sel.png"));
    textures.room_sel_distant = loadTexture(getPixmapFilenameRaw("room-sel-distant.png"));
    textures.room_sel_move_bad = loadTexture(getPixmapFilenameRaw("room-sel-move-bad.png"));
    textures.room_sel_move_good = loadTexture(getPixmapFilenameRaw("room-sel-move-good.png"));
    // 256
    textures.room_needs_update = loadTexture(getPixmapFilenameRaw("room-needs-update.png"));
    textures.room_modified = loadTexture(getPixmapFilenameRaw("room-modified.png"));

    {
        textures.for_each([this](SharedMMTexture &pTex) -> void {
            auto &tex = deref(pTex);
            assert(tex.get()); // make sure we didn't forget to initialize one

            auto id = allocateTextureId();
            tex.setId(id);
            m_opengl.setTextureLookup(id, pTex);
        });
    }

    updateTextures();

    // Initialize icon texture array
    // These are overlays, so ClamptoEdge is likely better. Min/MagLinear is fine.
    // No mipmaps for icons for now.
    constexpr int ICON_WIDTH = 32;
    constexpr int ICON_HEIGHT = 32;

    struct IconData {
        QImage image;
        std::optional<RoomMobFlagEnum> mob_flag;
        std::optional<RoomLoadFlagEnum> load_flag;
        bool is_no_ride = false;
        // Add other variants if necessary for other icon types
    };
    std::vector<IconData> icon_images;
    int current_layer_index = 0;

    // Helper to load and prepare icon image
    auto load_and_prepare_icon = [&](const QString& filename, bool mirror_image = true) -> QImage {
        QImage img(filename);
        if (img.isNull()) {
            qWarning() << "Failed to load icon image:" << filename;
            // Return a dummy image
            img = QImage(ICON_WIDTH, ICON_HEIGHT, QImage::Format_RGBA8888);
            img.fill(Qt::magenta); // Magenta = error
            return img;
        }
        if (mirror_image) {
            img = img.mirrored();
        }
        if (img.size() != QSize(ICON_WIDTH, ICON_HEIGHT)) {
            img = img.scaled(ICON_WIDTH, ICON_HEIGHT, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        }
        return img.convertToFormat(QImage::Format_RGBA8888);
    };

    // Collect Mob Icons
    for (size_t i = 0u; i < textures.mob.size(); ++i) {
        const auto flag = static_cast<RoomMobFlagEnum>(i);
        // We still load the individual textures as before for now
        // textures.mob[flag] = loadTexture(getPixmapFilename(flag));
        // If textures.mob[flag] is already loaded, we could potentially get the QImage from it,
        // but QOpenGLTexture doesn't provide a direct way back to QImage.
        // So, we load the QImage again.
        QImage mob_image = load_and_prepare_icon(getPixmapFilename(flag));
        icon_images.push_back({mob_image, flag, std::nullopt, false});
        textures.mob_icon_layers[flag] = current_layer_index++;
    }

    // Collect Load Icons
    for (size_t i = 0u; i < textures.load.size(); ++i) {
        const auto flag = static_cast<RoomLoadFlagEnum>(i);
        // textures.load[flag] = loadTexture(getPixmapFilename(flag)); // Still loaded above
        QImage load_image = load_and_prepare_icon(getPixmapFilename(flag));
        icon_images.push_back({load_image, std::nullopt, flag, false});
        textures.load_icon_layers[flag] = current_layer_index++;
    }

    // Collect NoRide Icon
    // textures.no_ride = loadTexture(getPixmapFilenameRaw("no-ride.png")); // Still loaded above
    QImage no_ride_image = load_and_prepare_icon(getPixmapFilenameRaw("no-ride.png"));
    icon_images.push_back({no_ride_image, std::nullopt, std::nullopt, true});
    textures.no_ride_icon_layer = current_layer_index++;

    // TODO: Collect other overlay icons like room_sel_distant, room_needs_update, room_modified
    // For example:
    // QImage room_needs_update_image = load_and_prepare_icon(getPixmapFilenameRaw("room-needs-update.png"));
    // icon_images.push_back({room_needs_update_image, ...});
    // textures.room_needs_update_icon_layer = current_layer_index++; // (Need to add this member to MapCanvasTextures)


    const int total_icons = static_cast<int>(icon_images.size());

    if (total_icons > 0) {
        textures.icon_texture_array = MMTexture::alloc(
            QOpenGLTexture::Target2DArray,
            [&](QOpenGLTexture &tex_array) {
                // tex_array is already created by MMTexture::alloc's internal new QOpenGLTexture(target)
                // but we need to configure it for 2D Array
                tex_array.create(); // Ensure it's created if MMTexture didn't do it (it should)

                tex_array.setSize(ICON_WIDTH, ICON_HEIGHT, total_icons); // Width, Height, Depth (layers)
                tex_array.setFormat(QOpenGLTexture::RGBA8_UNorm);
                tex_array.setWrapMode(QOpenGLTexture::ClampToEdge);
                tex_array.setMinMagFilters(QOpenGLTexture::Linear, QOpenGLTexture::Linear);
                tex_array.setMipLevels(1); // No mipmaps for these icons for now
                tex_array.allocateStorage();

                for (const auto& icon_data : icon_images) {
                    int layer_idx = -1;
                    if (icon_data.is_no_ride) {
                        layer_idx = textures.no_ride_icon_layer.value();
                    } else if (icon_data.mob_flag.has_value()) {
                        layer_idx = textures.mob_icon_layers[icon_data.mob_flag.value()];
                    } else if (icon_data.load_flag.has_value()) {
                        layer_idx = textures.load_icon_layers[icon_data.load_flag.value()];
                    }
                    // Add conditions for other icon types if they are added

                    if (layer_idx != -1) {
                        if (!icon_data.image.isNull()) {
                             // setData(level, layer, QImage)
                            tex_array.setData(0, layer_idx, icon_data.image);
                        } else {
                            qWarning() << "Null image for layer" << layer_idx;
                        }
                    } else {
                        qWarning() << "Layer index not found for an icon image.";
                    }
                }
            },
            true // forbidUpdates = true, as we populate it once here.
        );
        // The icon_texture_array will be assigned an ID in the for_each loop below
    }

    // After all textures have their IDs assigned by the loop below,
    // populate the individual_texture_to_array_layer map.
    // This needs to be done after the main for_each loop.
    // We'll add a section later for this.
}

namespace mctp {

namespace detail {

static MMTextureId copy_proxy(const SharedMMTexture &pTex)
{
    if (!pTex) {
        return INVALID_MM_TEXTURE_ID;
    }

    MMTexture &tex = *pTex;
    auto id = tex.getId();

    if ((false)) {
        MAYBE_UNUSED QOpenGLTexture *qtex = tex.get();
        QOpenGLTexture::Target target = tex.target();
        bool canBeUpdated = tex.canBeUpdated();

        qInfo() << "target:" << target << "/ id:" << id.value()
                << "/ canBeUpdated:" << canBeUpdated;
    }

    return id;
}

template<typename T>
auto copy_proxy(const T &input) -> ::mctp::detail::proxy_t<T>
{
    using Output = ::mctp::detail::proxy_t<T>;
    using E = typename Output::index_type;
    constexpr size_t SIZE = Output::SIZE;
    Output output;
    for (size_t i = 0; i < SIZE; ++i) {
        const auto e = static_cast<E>(i);
        output[e] = copy_proxy(input[e]);
    }
    return output;
}

} // namespace detail

MapCanvasTexturesProxy getProxy(const MapCanvasTextures &mct)
{
    MapCanvasTexturesProxy result;
#define X_COPY_PROXY(_Type, _Name) result._Name = detail::copy_proxy(mct._Name);
    XFOREACH_MAPCANVAS_TEXTURES(X_COPY_PROXY)
#undef X_COPY_PROXY
    return result;
}
} // namespace mctp

void MapCanvas::updateTextures()
{
    const bool wantTrilinear = getConfig().canvas.trilinearFiltering;
    std::optional<bool> &activeStatus = m_graphicsOptionsStatus.trilinear;
    if (activeStatus == wantTrilinear) {
        return;
    }

    m_textures.for_each([wantTrilinear](SharedMMTexture &tex) -> void {
        if (tex->canBeUpdated()) {
            ::setTrilinear(tex, wantTrilinear);
        }
    });
    activeStatus = wantTrilinear;

    // called to trigger an early error
    std::ignore = mctp::getProxy(m_textures);

    // Populate the individual_texture_to_array_layer map
    if (textures.icon_texture_array) { // Only if the array was created
        for (size_t i = 0u; i < textures.mob.size(); ++i) {
            const auto flag = static_cast<RoomMobFlagEnum>(i);
            if (textures.mob[flag] && textures.mob[flag]->getId() != INVALID_MM_TEXTURE_ID) {
                auto it = textures.mob_icon_layers.find(flag);
                if (it != textures.mob_icon_layers.end()) {
                    textures.individual_texture_to_array_layer[textures.mob[flag]->getId()] = it->second;
                }
            }
        }

        for (size_t i = 0u; i < textures.load.size(); ++i) {
            const auto flag = static_cast<RoomLoadFlagEnum>(i);
            if (textures.load[flag] && textures.load[flag]->getId() != INVALID_MM_TEXTURE_ID) {
                auto it = textures.load_icon_layers.find(flag);
                if (it != textures.load_icon_layers.end()) {
                    textures.individual_texture_to_array_layer[textures.load[flag]->getId()] = it->second;
                }
            }
        }

        if (textures.no_ride && textures.no_ride->getId() != INVALID_MM_TEXTURE_ID && textures.no_ride_icon_layer.has_value()) {
            textures.individual_texture_to_array_layer[textures.no_ride->getId()] = textures.no_ride_icon_layer.value();
        }

        // TODO: Add other icons here if they are part of the array
        // Example:
        // if (textures.room_needs_update && textures.room_needs_update->getId() != INVALID_MM_TEXTURE_ID && textures.room_needs_update_icon_layer.has_value()) {
        //     textures.individual_texture_to_array_layer[textures.room_needs_update->getId()] = textures.room_needs_update_icon_layer.value();
        // }
    }
}
