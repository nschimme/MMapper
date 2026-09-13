// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#pragma once

#include <cstdint>
#include <set>
#include <unordered_map>
#include <vector>

#include <QtCore/QString>
#include <QtGui/QFont>
#include <QtGui/QImage>

namespace font_gen {

struct GlyphMetrics final
{
    char32_t id = 0;
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    int xoffset = 0;
    int yoffset = 0;
    int xadvance = 0;
    int page = 0;
};

struct FontAtlasData final
{
    bool success = false;
    QString errorMessage;
    int lineHeight = 0;
    int base = 0;
    int scaleW = 0;
    int scaleH = 0;
    std::unordered_map<char32_t, GlyphMetrics> glyphs;
    std::vector<QImage> texturePages;
};

class FontGenerator final
{
public:
    FontGenerator() = default;
    ~FontGenerator() = default;

    // Default character set including Latin-1 range (32-126, 160-255) and MUD map indicators
    static std::set<char32_t> getDefaultCharSet();

    // Generate in-memory FontAtlasData directly
    static FontAtlasData generateAtlas(const QFont &font,
                                       const std::set<char32_t> &chars = getDefaultCharSet());

    static FontAtlasData generateAtlas(const QString &fontFamily,
                                       int pointSize,
                                       const std::set<char32_t> &chars = getDefaultCharSet());
};

} // namespace font_gen
