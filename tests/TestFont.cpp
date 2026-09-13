// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#include "../src/configuration/configuration.h"
#include "../src/font/FontGenerator.h"

#include <QtGui/QFontDatabase>
#include <QtTest/QtTest>

class TestFont final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        setEnteredMain();
    }

    void testInMemoryFontGenerator()
    {
        const QString family = QStringLiteral("DejaVu Sans");
        const int size = 18;
        std::set<char32_t> chars = {32, 65, 66, 67, 0x1F9D9 /* Wizard */, 0x26F5 /* Boat */};

        const auto atlas = font_gen::FontGenerator::generateAtlas(family, size, chars);

        QVERIFY(atlas.success);
        QVERIFY(atlas.lineHeight > 0);
        QVERIFY(atlas.base > 0);
        QCOMPARE(atlas.scaleW, 512);
        QCOMPARE(atlas.scaleH, 512);
        QCOMPARE(atlas.glyphs.size(), static_cast<size_t>(6));
        QVERIFY(atlas.glyphs.find(65) != atlas.glyphs.end());      // 'A'
        QVERIFY(atlas.glyphs.find(0x1F9D9) != atlas.glyphs.end()); // Wizard

        QVERIFY(!atlas.texturePages.empty());
        const QImage &page0 = atlas.texturePages[0];
        QCOMPARE(page0.width(), 512);
        QCOMPARE(page0.height(), 512);
        QCOMPARE(page0.format(), QImage::Format_ARGB32);
    }

    void testDefaultCharSetCoverage()
    {
        const auto chars = font_gen::FontGenerator::getDefaultCharSet();

        // Latin-1 basics.
        QVERIFY(chars.contains(U'A'));
        QVERIFY(chars.contains(U'~'));

        // Common English typography beyond Latin-1 (em/en dash, curly
        // quotes, ellipsis) that a bare Latin-1 range would miss.
        QVERIFY(chars.contains(U'—')); // em dash
        QVERIFY(chars.contains(U'–')); // en dash
        QVERIFY(chars.contains(U'‘')); // left single quote
        QVERIFY(chars.contains(U'’')); // right single quote
        QVERIFY(chars.contains(U'“')); // left double quote
        QVERIFY(chars.contains(U'”')); // right double quote
        QVERIFY(chars.contains(U'…')); // ellipsis

        // MUD map indicator emoji.
        QVERIFY(chars.contains(U'⚡')); // lightning bolt
    }

    void testColorGlyphFlag()
    {
        const std::set<char32_t> chars = {U'A', U'—' /* em dash */, U'⚡' /* lightning */};
        const auto atlas = font_gen::FontGenerator::generateAtlas(QStringLiteral("DejaVu Sans"),
                                                                    18,
                                                                    chars);
        QVERIFY(atlas.success);

        // Plain letters and punctuation are monochrome SDF glyphs...
        QVERIFY(!atlas.glyphs.at(U'A').isColor);
        QVERIFY(!atlas.glyphs.at(U'—').isColor);
        // ...while curated emoji/indicators render as full color.
        QVERIFY(atlas.glyphs.at(U'⚡').isColor);
    }

    void testGlyphPaddingIsBakedConsistently()
    {
        const auto atlas = font_gen::FontGenerator::generateAtlas(QStringLiteral("DejaVu Sans"),
                                                                    18,
                                                                    {U'A'});
        QVERIFY(atlas.success);
        // Padding must be positive (SDF glyphs need spread room) and small
        // relative to a normal glyph, or callers subtracting it out (see
        // FontAtlasData::glyphPadding's doc comment) would go negative.
        QVERIFY(atlas.glyphPadding > 0);
        const auto &a = atlas.glyphs.at(U'A');
        QVERIFY(a.width > atlas.glyphPadding * 2);
    }

    void testKerningPairsGenerated()
    {
        // A full default-charset atlas for a real font should produce at
        // least some non-zero kerning pairs (e.g. "AV", "To"); this guards
        // against the kerning table silently going empty.
        const auto atlas = font_gen::FontGenerator::generateAtlas(
            QStringLiteral("DejaVu Sans"), 18, font_gen::FontGenerator::getDefaultCharSet());
        QVERIFY(atlas.success);
        QVERIFY(!atlas.kernings.empty());
        for (const auto &k : atlas.kernings) {
            QVERIFY(k.amount != 0);
        }
    }

    void testCantarellMatchesMasterSizeConvention()
    {
        // Master's baked BMFont assets (e.g. Cantarell18.fnt, still in git
        // history at commit 48b6476ce^) always had common.lineHeight ==
        // info.size, i.e. "size" meant the target cell height, not a point
        // size or a value to feed straight to a rasterizer. Pin that
        // convention here so a regression (previously: requesting size 18
        // rendered at ~1.4x, matching neither this test's tolerance nor
        // master's look) is caught automatically instead of only by
        // eyeballing screenshots.
        const QString ttfPath = QFINDTESTDATA("../src/resources/fonts/Cantarell-Regular.ttf");
        QVERIFY(!ttfPath.isEmpty());
        const int fontId = QFontDatabase::addApplicationFont(ttfPath);
        QVERIFY(fontId != -1);
        const QString family = QFontDatabase::applicationFontFamilies(fontId).at(0);

        constexpr int requestedSize = 18;
        const auto atlas = font_gen::FontGenerator::generateAtlas(family, requestedSize);
        QVERIFY(atlas.success);

        // Master: lineHeight=18, base=15, "Black Hill" width=54 at size 18.
        QVERIFY(std::abs(atlas.lineHeight - requestedSize) <= 1);
        QVERIFY(std::abs(atlas.base - 15) <= 2);

        int width = 0;
        for (const QChar &qc : QStringLiteral("Black Hill")) {
            const auto it = atlas.glyphs.find(qc.unicode());
            if (it != atlas.glyphs.end()) {
                width += it->second.xadvance;
            }
        }
        QVERIFY(width > 0);
        // Within 15% of master's 54px: catches a gross scale regression
        // (the previous bug was ~40% over) without pinning to a specific
        // rasterizer's exact hinting/rounding.
        QVERIFY(std::abs(width - 54) <= 8);
    }
};

QTEST_MAIN(TestFont)
#include "TestFont.moc"
