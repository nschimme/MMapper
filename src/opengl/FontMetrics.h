#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "../global/Consts.h"
#include "../global/hash.h"
#include "../global/utils.h"
#include "GLText.h"
#include "OpenGLTypes.h"

#include <optional>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>

#include <QDebug>
#include <QImage>
#include <QString>

using IntPair = std::pair<int, int>;

template<>
struct std::hash<IntPair>
{
    std::size_t operator()(const IntPair &ip) const noexcept
    {
#define CAST(i) static_cast<uint64_t>(static_cast<uint32_t>(i))
        return numeric_hash(CAST(ip.first) | (CAST(ip.second) << 32u));
#undef CAST
    }
};

struct NODISCARD FontMetrics final
{
    static constexpr int UNDERLINE_ID = -257;
    static constexpr int BACKGROUND_ID = -258;

    struct NODISCARD Glyph final
    {
        int id = 0;
        int x = 0;
        int y = 0;
        int width = 0;
        int height = 0;
        int xoffset = 0;
        int yoffset = 0;
        int xadvance = 0;

        Glyph() = default;
        ~Glyph() = default;
        DEFAULT_CTORS_AND_ASSIGN_OPS(Glyph);

        // used by most cases
        explicit Glyph(const int id_,
                       const int x_,
                       const int y_,
                       const int width_,
                       const int height_,
                       const int xoffset_,
                       const int yoffset_,
                       const int xadvance_)
            : id{id_}
            , x{x_}
            , y{y_}
            , width{width_}
            , height{height_}
            , xoffset{xoffset_}
            , yoffset{yoffset_}
            , xadvance{xadvance_}
        {}

        // used for underline
        explicit Glyph(const int id_,
                       const int x_,
                       const int y_,
                       const int width_,
                       const int height_,
                       const int xoffset_,
                       const int yoffset_)
            : id{id_}
            , x{x_}
            , y{y_}
            , width{width_}
            , height{height_}
            , xoffset{xoffset_}
            , yoffset{yoffset_}
        {}

        // used for background
        explicit Glyph(const int id_, const int x_, const int y_, const int width_, const int height_)
            : id{id_}
            , x{x_}
            , y{y_}
            , width{width_}
            , height{height_}
        {}

        NODISCARD glm::ivec2 getPosition() const { return glm::ivec2{x, y}; }
        NODISCARD glm::ivec2 getSize() const { return glm::ivec2{width, height}; }
        NODISCARD glm::ivec2 getOffset() const { return glm::ivec2{xoffset, yoffset}; }

        NODISCARD utils::Rect getRect() const
        {
            const auto lo = getPosition();
            return utils::Rect{lo, lo + getSize()};
        }
    };

    struct NODISCARD Kerning final
    {
        int first = 0;
        int second = 0;
        int amount = 0;

        Kerning() = default;
        ~Kerning() = default;
        DEFAULT_CTORS_AND_ASSIGN_OPS(Kerning);

        Kerning(const int first_, const int second_, const int amount_)
            : first{first_}
            , second{second_}
            , amount{amount_}
        {}
    };

    struct NODISCARD Common final
    {
        int lineHeight = 0;
        int base = 0;
        int scaleW = 0;
        int scaleH = 0;
        int marginX = 0;
        int marginY = 0;
    };

    std::optional<Glyph> background;
    std::optional<Glyph> underline;

    Common common;

    std::vector<Glyph> raw_glyphs;
    std::vector<Kerning> raw_kernings;
    std::unordered_map<int, const Glyph *> glyphs;
    std::unordered_map<IntPair, const Kerning *> kernings;

    NODISCARD QString init(const QString &);

    NODISCARD const Glyph *lookupGlyph(const int i) const
    {
        const auto it = glyphs.find(i);
        return (it == glyphs.end()) ? nullptr : it->second;
    }

    NODISCARD const Glyph *lookupGlyph(const char c) const
    {
        return lookupGlyph(static_cast<int>(static_cast<unsigned char>(c)));
    }

    NODISCARD const Glyph *getBackground() const
    {
        return background ? &background.value() : nullptr;
    }
    NODISCARD const Glyph *getUnderline() const { return underline ? &underline.value() : nullptr; }

    NODISCARD bool tryAddBackgroundGlyph(QImage &img);
    NODISCARD bool tryAddUnderlineGlyph(QImage &img);
    void tryAddSyntheticGlyphs(QImage &img);

    NODISCARD const Kerning *lookupKerning(const Glyph *const prev, const Glyph *const current) const
    {
        if (prev == nullptr || current == nullptr) {
            return nullptr;
        }

        const auto it = kernings.find(IntPair{prev->id, current->id});
        return (it == kernings.end()) ? nullptr : it->second;
    }

    template<typename EmitGlyph>
    void foreach_glyph(const std::string_view msg, EmitGlyph &&emitGlyph) const
    {
        const Glyph *prev = nullptr;
        for (const char &c : msg) {
            const Glyph *const current = lookupGlyph(c);
            if (current != nullptr) {
                emitGlyph(current, lookupKerning(prev, current));
                prev = current;
            } else if (auto oops = lookupGlyph(char_consts::C_QUESTION_MARK)) {
                qWarning() << "Unable to lookup glyph" << QString(QChar(c));
                emitGlyph(oops, lookupKerning(prev, oops));
                prev = oops;
            } else {
                prev = nullptr;
            }
        }
    }

    NODISCARD int measureWidth(const std::string_view msg) const
    {
        int width = 0;
        foreach_glyph(msg, [&width](const Glyph *const g, const Kerning *const k) {
            width += g->xadvance;
            if (k != nullptr) {
                // kerning amount is added to the advance
                width += k->amount;
            }
        });
        return width;
    }

    void getFontBatchRawData(const GLText *text,
                             size_t count,
                             std::vector<::FontVert3d> &output) const;
};
