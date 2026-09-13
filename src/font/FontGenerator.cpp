// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#include "FontGenerator.h"

#include <algorithm>
#include <cmath>

#include <QtGui/QFontMetrics>
#include <QtGui/QPainter>

namespace font_gen {

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
    // Common MUD Emojis / Map Indicators
    const std::vector<char32_t> extraEmojis = {// Moon phases & Weather
                                               0x1F311,
                                               0x1F312,
                                               0x1F313,
                                               0x1F314,
                                               0x1F315,
                                               0x1F316,
                                               0x1F317,
                                               0x1F318,
                                               0x2600,
                                               0x2601,
                                               0x1F324,
                                               0x1F325,
                                               0x1F326,
                                               0x1F327,
                                               0x1F328,
                                               0x26A1,
                                               0x1F32B,
                                               0x1F301,
                                               // Indicators
                                               0x26A0,
                                               0x1F44D,
                                               0x1F4AF,
                                               0x1F170,
                                               0x1F480,
                                               0x2764,
                                               // Wizard, Magic, Scrolls
                                               0x1F9D9,
                                               0x2728,
                                               0x1F52E,
                                               0x1F4DC,
                                               // Troll, Ogre, Goblin, Combat
                                               0x1F9CC,
                                               0x1F479,
                                               0x1F47A,
                                               0x1F5E1,
                                               0x2694,
                                               0x1F3F9,
                                               0x1F6E1,
                                               // Food, Drink
                                               0x1F35E,
                                               0x1F356,
                                               0x1F37A,
                                               0x1F377,
                                               // Boat, Travel, Mount
                                               0x26F5,
                                               0x1F6A3,
                                               0x1F6A2,
                                               0x1F40E,
                                               // Places & Loot
                                               0x1F3F0,
                                               0x26FA,
                                               0x1F511,
                                               0x1F4B0,
                                               0x1F3F3};
    for (char32_t c : extraEmojis) {
        chars.insert(c);
    }
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

    int page = 0;
    int x = 0;
    int y = 0;
};

// Compute Signed Distance Field (SDF) image from high-resolution rendered glyph
static QImage generateSdfGlyph(const QImage &highResImg, int scaleFactor, int spread)
{
    const int targetW = highResImg.width() / scaleFactor;
    const int targetH = highResImg.height() / scaleFactor;

    QImage sdfImg(targetW, targetH, QImage::Format_ARGB32);
    sdfImg.fill(Qt::transparent);

    const int srcW = highResImg.width();
    const int srcH = highResImg.height();

    const float spreadSrc = static_cast<float>(spread * scaleFactor);
    const int searchRadius = std::max(1, static_cast<int>(std::ceil(spreadSrc)));

    for (int ty = 0; ty < targetH; ++ty) {
        QRgb *dstLine = reinterpret_cast<QRgb *>(sdfImg.scanLine(ty));
        const int cy = std::clamp(static_cast<int>((ty + 0.5f) * scaleFactor), 0, srcH - 1);
        const QRgb *srcCenterLine = reinterpret_cast<const QRgb *>(highResImg.constScanLine(cy));

        for (int tx = 0; tx < targetW; ++tx) {
            const int cx = std::clamp(static_cast<int>((tx + 0.5f) * scaleFactor), 0, srcW - 1);
            const bool isInside = (qAlpha(srcCenterLine[cx]) > 127);

            float minSqDist = spreadSrc * spreadSrc;

            const int minY = std::max(0, cy - searchRadius);
            const int maxY = std::min(srcH - 1, cy + searchRadius);
            const int minX = std::max(0, cx - searchRadius);
            const int maxX = std::min(srcW - 1, cx + searchRadius);

            for (int sy = minY; sy <= maxY; ++sy) {
                const QRgb *srcRow = reinterpret_cast<const QRgb *>(highResImg.constScanLine(sy));
                const float dy = sy - cy;
                for (int sx = minX; sx <= maxX; ++sx) {
                    const bool sampleInside = (qAlpha(srcRow[sx]) > 127);
                    if (sampleInside != isInside) {
                        const float dx = sx - cx;
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

    for (char32_t c : chars) {
        const QString str = QString::fromStdU32String(std::u32string(1, c));

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
        const int basePtSize = renderFont.pointSize() > 0 ? renderFont.pointSize() : 18;
        highResFont.setPointSize(basePtSize * sdfScale);
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
        painter.setPen(Qt::white);

        const int drawX = highResPadding - highResBbox.left();
        const int drawY = highResPadding - highResBbox.top();
        painter.drawText(drawX, drawY, str);
        painter.end();

        QImage glyphImg = generateSdfGlyph(highResImg, sdfScale, sdfSpread);

        PackGlyph g;
        g.id = c;
        g.width = renderW;
        g.height = renderH;
        g.xoffset = bbox.left() - padding;
        g.yoffset = ascent + bbox.top() - padding;
        g.xadvance = advance;
        g.image = glyphImg;

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

    atlas.scaleW = texW;
    atlas.scaleH = texH;

    int currentY = 0;
    int currentX = 0;
    int currentShelfH = 0;

    std::vector<QImage> pages;
    pages.emplace_back(texW, texH, QImage::Format_ARGB32);
    pages.back().fill(Qt::transparent);

    {
        QPainter pagePainter(&pages[0]);
        for (PackGlyph &g : glyphs) {
            if (currentX + g.width + spacing > texW) {
                currentY += currentShelfH + spacing;
                currentX = 0;
                currentShelfH = 0;
            }

            g.page = 0;
            g.x = currentX;
            g.y = currentY;

            pagePainter.drawImage(g.x, g.y, g.image);

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
            atlas.glyphs[g.id] = gm;
        }
    }

    atlas.success = true;
    atlas.texturePages = std::move(pages);
    return atlas;
}

FontAtlasData FontGenerator::generateAtlas(const QString &fontFamily,
                                           int pointSize,
                                           const std::set<char32_t> &chars)
{
    QFont font(fontFamily, pointSize);
    return generateAtlas(font, chars);
}

} // namespace font_gen
