// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#include "FontGenerator.h"

#include <algorithm>
#include <cmath>

#include <QtCore/QDebug>
#include <QtGui/QFontMetrics>
#include <QtGui/QPainter>

namespace font_gen {

// Common MUD Emojis / Map Indicators. These are rendered as full-color glyphs
// rather than being flattened into the monochrome signed distance field.
static const std::set<char32_t> &getEmojiCharSet()
{
    static const std::set<char32_t> emojis = {
        // Moon phases & Weather
        0x1F311, 0x1F312, 0x1F313, 0x1F314, 0x1F315, 0x1F316, 0x1F317, 0x1F318,
        0x2600,  0x2601,  0x1F324, 0x1F325, 0x1F326, 0x1F327, 0x1F328, 0x26A1, 0x1F32B, 0x1F301,
        // Indicators
        0x26A0,  0x1F44D, 0x1F4AF, 0x1F170, 0x1F480, 0x2764,
        // Wizard, Magic, Scrolls
        0x1F9D9, 0x2728,  0x1F52E, 0x1F4DC,
        // Troll, Ogre, Goblin, Combat
        0x1F9CC, 0x1F479, 0x1F47A, 0x1F5E1, 0x2694,  0x1F3F9, 0x1F6E1,
        // Food, Drink
        0x1F35E, 0x1F356, 0x1F37A, 0x1F377,
        // Boat, Travel, Mount
        0x26F5,  0x1F6A3, 0x1F6A2, 0x1F40E,
        // Places & Loot
        0x1F3F0, 0x26FA,  0x1F511, 0x1F4B0, 0x1F3F3
    };
    return emojis;
}

// General Punctuation (U+2000-U+206F) characters that show up regularly in
// English prose: dashes, curly quotes, ellipsis, bullet, dagger, etc. These
// stay monochrome text glyphs (tinted by the caller's color), not color emoji.
static const std::set<char32_t> &getExtendedPunctuationCharSet()
{
    static const std::set<char32_t> punctuation = {
        0x2010, 0x2011, 0x2012, 0x2013, 0x2014, // hyphen, non-breaking hyphen, figure/en/em dash
        0x2018, 0x2019, 0x201A, 0x201C, 0x201D, 0x201E, // single/double curly quotes
        0x2020, 0x2021, // dagger, double dagger
        0x2022,         // bullet
        0x2026,         // horizontal ellipsis
        0x2030,         // per mille
        0x2032, 0x2033, // prime, double prime
        0x2039, 0x203A, // single guillemets
        0x2044,         // fraction slash
        0x20AC,         // euro sign
        0x2122,         // trademark
    };
    return punctuation;
}

std::set<char32_t> FontGenerator::getDefaultCharSet()
{
    std::set<char32_t> chars;
    // Basic Latin / ASCII (32 to 126)
    for (char32_t c = 32; c <= 126; ++c) {
        chars.insert(c);
    }
    // Latin-1 Supplement (160 to 255)
    for (char32_t c = 160; c <= 255; ++c) {
        chars.insert(c);
    }
    chars.insert(getExtendedPunctuationCharSet().begin(), getExtendedPunctuationCharSet().end());
    chars.insert(getEmojiCharSet().begin(), getEmojiCharSet().end());
    return chars;
}

struct PackGlyph final
{
    char32_t id = 0;
    int width = 0;
    int height = 0;
    int xoffset = 0;
    int yoffset = 0;
    int xadvance = 0;
    QImage image;
    bool isColor = false;

    int page = 0;
    int x = 0;
    int y = 0;
};

// Only the hand-picked MUD emoji/indicators (see getEmojiCharSet()) are
// rendered as full-color glyphs; everything else -- including extended
// punctuation like em-dash or curly quotes -- stays a monochrome distance
// field glyph tinted by the caller's text color.
static bool isColorGlyph(char32_t c)
{
    return getEmojiCharSet().contains(c);
}

// Compute Signed Distance Field (SDF) image from high-resolution rendered glyph.
// The SDF value is stored in the alpha channel (RGB is left at opaque white);
// callers that want a plain color glyph should not route through here.
static QImage generateSdfGlyph(const QImage &highResImg, int scaleFactor, int spread)
{
    const int targetW = highResImg.width() / scaleFactor;
    const int targetH = highResImg.height() / scaleFactor;

    QImage sdfImg(targetW, targetH, QImage::Format_ARGB32);
    sdfImg.fill(Qt::transparent);

    const int srcW = highResImg.width();
    const int srcH = highResImg.height();
    const float scaleFactorF = static_cast<float>(scaleFactor);

    const float spreadSrc = static_cast<float>(spread) * scaleFactorF;
    const int searchRadius = std::max(1, static_cast<int>(std::ceil(spreadSrc)));

    for (int ty = 0; ty < targetH; ++ty) {
        QRgb *dstLine = reinterpret_cast<QRgb *>(sdfImg.scanLine(ty));
        const int cy = std::clamp(static_cast<int>((static_cast<float>(ty) + 0.5f) * scaleFactorF),
                                  0,
                                  srcH - 1);
        const QRgb *srcCenterLine = reinterpret_cast<const QRgb *>(highResImg.constScanLine(cy));

        for (int tx = 0; tx < targetW; ++tx) {
            const int cx = std::clamp(static_cast<int>((static_cast<float>(tx) + 0.5f) * scaleFactorF),
                                      0,
                                      srcW - 1);
            const bool isInside = (qAlpha(srcCenterLine[cx]) > 127);

            float minSqDist = spreadSrc * spreadSrc;

            const int minY = std::max(0, cy - searchRadius);
            const int maxY = std::min(srcH - 1, cy + searchRadius);
            const int minX = std::max(0, cx - searchRadius);
            const int maxX = std::min(srcW - 1, cx + searchRadius);

            for (int sy = minY; sy <= maxY; ++sy) {
                const QRgb *srcRow = reinterpret_cast<const QRgb *>(highResImg.constScanLine(sy));
                const float dy = static_cast<float>(sy - cy);
                for (int sx = minX; sx <= maxX; ++sx) {
                    const bool sampleInside = (qAlpha(srcRow[sx]) > 127);
                    if (sampleInside != isInside) {
                        const float dx = static_cast<float>(sx - cx);
                        const float sqDist = dx * dx + dy * dy;
                        if (sqDist < minSqDist) {
                            minSqDist = sqDist;
                        }
                    }
                }
            }

            const float dist = std::sqrt(minSqDist);
            const float signedDist = isInside ? dist : -dist;

            float normDist = 0.5f + (signedDist / (2.0f * spreadSrc));
            normDist = std::clamp(normDist, 0.0f, 1.0f);

            const uint8_t alphaVal = static_cast<uint8_t>(std::round(normDist * 255.0f));
            dstLine[tx] = qRgba(255, 255, 255, alphaVal);
        }
    }

    return sdfImg;
}

// Downscale a high-resolution color glyph (e.g. an emoji) directly to the
// target size, preserving its actual RGBA color instead of collapsing it to
// a monochrome distance field.
static QImage generateColorGlyph(const QImage &highResImg, int scaleFactor)
{
    const int targetW = std::max(1, highResImg.width() / scaleFactor);
    const int targetH = std::max(1, highResImg.height() / scaleFactor);
    return highResImg.scaled(targetW,
                             targetH,
                             Qt::IgnoreAspectRatio,
                             Qt::SmoothTransformation)
        .convertToFormat(QImage::Format_ARGB32);
}

FontAtlasData FontGenerator::generateAtlas(const QFont &font,
                                            const std::set<char32_t> &selectedChars)
{
    FontAtlasData atlas;
    std::set<char32_t> chars = selectedChars;
    if (chars.empty()) {
        chars = getDefaultCharSet();
    }

    QFont renderFont = font;
    renderFont.setStyleStrategy(QFont::PreferAntialias);

    QFontMetrics fm(renderFont);
    const int fontHeight = fm.height();
    const int ascent = fm.ascent();

    atlas.lineHeight = fontHeight;
    atlas.base = ascent;
    atlas.scaleW = 512;
    atlas.scaleH = 512;

    std::vector<PackGlyph> glyphs;
    glyphs.reserve(chars.size());

    constexpr int padding = 4;
    constexpr int sdfScale = 4;
    constexpr int sdfSpread = 4;
    atlas.glyphPadding = padding;

    for (char32_t c : chars) {
        const bool colorGlyph = isColorGlyph(c);
        // Many of our color glyphs (e.g. U+26A1 lightning bolt, U+26A0 warning
        // sign) default to a plain black-and-white *text* presentation glyph;
        // appending U+FE0F (VARIATION SELECTOR-16) forces the full-color emoji
        // presentation. It's a no-op for codepoints that are already
        // emoji-presentation by default.
        const std::u32string codepoints = colorGlyph ? std::u32string{c, char32_t{0xFE0F}}
                                                      : std::u32string{c};
        const QString str = QString::fromStdU32String(codepoints);

        const QRect bbox = fm.boundingRect(str);
        const int advance = fm.horizontalAdvance(str);

        int w = bbox.width();
        int h = bbox.height();

        if (w <= 0) {
            w = std::max(1, advance);
        }
        if (h <= 0) {
            h = std::max(1, fontHeight);
        }

        const int renderW = w + padding * 2;
        const int renderH = h + padding * 2;

        QFont highResFont = renderFont;
        // A font built with setPixelSize() reports pointSize() == -1; guard against
        // scaling a negative/invalid point size.
        if (renderFont.pointSize() > 0) {
            highResFont.setPointSize(renderFont.pointSize() * sdfScale);
        } else {
            highResFont.setPixelSize(std::max(1, renderFont.pixelSize()) * sdfScale);
        }
        QFontMetrics highResFm(highResFont);
        const QRect highResBbox = highResFm.boundingRect(str);

        const int highResPadding = padding * sdfScale;
        const int highResW = renderW * sdfScale;
        const int highResH = renderH * sdfScale;

        QImage highResImg(highResW, highResH, QImage::Format_ARGB32);
        highResImg.fill(Qt::transparent);

        QPainter painter(&highResImg);
        painter.setFont(highResFont);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setRenderHint(QPainter::TextAntialiasing, true);
        // Color glyphs (e.g. emoji) ignore the pen and paint their own colors;
        // monochrome glyphs are painted white so only their SDF alpha matters.
        painter.setPen(colorGlyph ? Qt::black : Qt::white);

        const int drawX = highResPadding - highResBbox.left();
        const int drawY = highResPadding - highResBbox.top();
        painter.drawText(drawX, drawY, str);
        painter.end();

        QImage glyphImg = colorGlyph ? generateColorGlyph(highResImg, sdfScale)
                                      : generateSdfGlyph(highResImg, sdfScale, sdfSpread);

        PackGlyph g;
        g.id = c;
        g.width = renderW;
        g.height = renderH;
        g.xoffset = bbox.left() - padding;
        g.yoffset = ascent + bbox.top() - padding;
        g.xadvance = advance;
        g.image = glyphImg;
        g.isColor = colorGlyph;

        glyphs.push_back(g);
    }

    std::sort(glyphs.begin(), glyphs.end(), [](const PackGlyph &a, const PackGlyph &b) {
        return a.height > b.height;
    });

    int texW = atlas.scaleW;
    int texH = atlas.scaleH;
    const int spacing = 1;

    auto calculatePacking = [&](int width, int height) -> bool {
        int cx = 0;
        int cy = 0;
        int shelfH = 0;
        for (const PackGlyph &g : glyphs) {
            if (cx + g.width + spacing > width) {
                cy += shelfH + spacing;
                cx = 0;
                shelfH = 0;
            }
            if (cy + g.height + spacing > height) {
                return false;
            }
            cx += g.width + spacing;
            shelfH = std::max(shelfH, g.height);
        }
        return true;
    };

    while (!calculatePacking(texW, texH) && texW < 2048 && texH < 2048) {
        texW *= 2;
        texH *= 2;
    }

    if (!calculatePacking(texW, texH)) {
        qWarning() << "Font atlas packing failed to fit" << glyphs.size()
                   << "glyphs even at the maximum" << texW << "x" << texH
                   << "texture size; some glyphs will be dropped";
    }

    atlas.scaleW = texW;
    atlas.scaleH = texH;

    int currentY = 0;
    int currentX = 0;
    int currentShelfH = 0;

    std::vector<QImage> pages;
    pages.emplace_back(texW, texH, QImage::Format_ARGB32);
    pages.back().fill(Qt::transparent);

    for (PackGlyph &g : glyphs) {
        if (currentX + g.width + spacing > texW) {
            currentY += currentShelfH + spacing;
            currentX = 0;
            currentShelfH = 0;
        }

        if (currentY + g.height + spacing > texH) {
            qWarning() << "Dropping glyph" << static_cast<uint32_t>(g.id)
                       << "because the font atlas is full";
            continue;
        }

        g.page = 0;
        g.x = currentX;
        g.y = currentY;

        QPainter pagePainter(&pages[0]);
        pagePainter.drawImage(g.x, g.y, g.image);
        pagePainter.end();

        currentX += g.width + spacing;
        currentShelfH = std::max(currentShelfH, g.height);

        GlyphMetrics gm;
        gm.id = g.id;
        gm.x = g.x;
        gm.y = g.y;
        gm.width = g.width;
        gm.height = g.height;
        gm.xoffset = g.xoffset;
        gm.yoffset = g.yoffset;
        gm.xadvance = g.xadvance;
        gm.page = 0;
        gm.isColor = g.isColor;
        atlas.glyphs[g.id] = gm;
    }

    // Compute kerning pairs for text glyphs (color/emoji glyphs never need kerning).
    // The old build-time BMFont pipeline baked kerning pairs from FreeType; the
    // live QFont path doesn't expose them directly, so derive each pair's
    // adjustment the same way AngelCode's tool effectively does: the difference
    // between the shaped pair's advance and the sum of the two glyphs' own advances.
    {
        std::vector<char32_t> textChars;
        textChars.reserve(atlas.glyphs.size());
        for (const auto &[id, gm] : atlas.glyphs) {
            if (!gm.isColor) {
                textChars.push_back(id);
            }
        }

        for (const char32_t first : textChars) {
            const auto firstIt = atlas.glyphs.find(first);
            if (firstIt == atlas.glyphs.end()) {
                continue;
            }
            const int firstAdvance = firstIt->second.xadvance;
            const QString firstStr = QString::fromStdU32String(std::u32string(1, first));

            for (const char32_t second : textChars) {
                const auto secondIt = atlas.glyphs.find(second);
                if (secondIt == atlas.glyphs.end()) {
                    continue;
                }
                const int secondAdvance = secondIt->second.xadvance;
                const QString pairStr = firstStr
                                        + QString::fromStdU32String(std::u32string(1, second));

                const int pairAdvance = fm.horizontalAdvance(pairStr);
                const int amount = pairAdvance - (firstAdvance + secondAdvance);
                if (amount != 0) {
                    atlas.kernings.push_back(KerningPair{first, second, amount});
                }
            }
        }
    }

    atlas.success = true;
    atlas.texturePages = std::move(pages);
    return atlas;
}

FontAtlasData FontGenerator::generateAtlas(const QString &fontFamily,
                                            int pixelSize,
                                            const std::set<char32_t> &chars)
{
    // pixelSize is meant as a physical-pixel *cell height* (matching BMFont's
    // <info size="N"/> convention, where master's baked fonts always had
    // common.lineHeight == info.size), not a point size and not the pixel
    // size to hand straight to QFont::setPixelSize(): Qt's fontmetrics height
    // for a given pixel size depends on the font's own ascent+descent, which
    // for Cantarell comes out ~1.4x the requested pixel size, not 1x. Probe
    // once and rescale so the resulting QFontMetrics::height() lands on the
    // requested cell height, matching master's convention (and avoiding the
    // ~33-40% oversized glyphs that came from treating pixelSize as if it
    // already equalled the desired line height).
    const int wantHeight = std::max(1, pixelSize);
    const auto heightAt = [&fontFamily](int px) -> int {
        QFont probe(fontFamily);
        probe.setPixelSize(std::max(1, px));
        return std::max(1, QFontMetrics(probe).height());
    };

    const int probeHeight = heightAt(wantHeight);
    const int firstGuess = std::max(1,
                                     static_cast<int>(std::lround(
                                         static_cast<double>(wantHeight) * wantHeight
                                         / probeHeight)));

    // The proportional guess above assumes height scales linearly with pixel
    // size, but QFontMetrics rounds ascent/descent to whole pixels at every
    // size, so it can land 1px off the target. Refine with a tiny local
    // search rather than accepting that rounding slop, so the requested size
    // (matching master's BMFont convention: lineHeight == requested size)
    // is hit as exactly as an integer pixel size allows.
    int bestPixelSize = firstGuess;
    int bestError = std::abs(heightAt(firstGuess) - wantHeight);
    for (int delta = -2; delta <= 2; ++delta) {
        if (delta == 0) {
            continue;
        }
        const int candidate = firstGuess + delta;
        if (candidate < 1) {
            continue;
        }
        const int error = std::abs(heightAt(candidate) - wantHeight);
        if (error < bestError) {
            bestError = error;
            bestPixelSize = candidate;
        }
    }

    QFont font(fontFamily);
    font.setPixelSize(bestPixelSize);
    return generateAtlas(font, chars);
}

} // namespace font_gen
