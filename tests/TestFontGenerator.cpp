// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#include "../src/configuration/configuration.h"
#include "../src/font/FontGenerator.h"
#include "../src/global/utils.h"

#include <QtTest/QtTest>
#include <QtWidgets/QApplication>

class TestFontGenerator final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase() { setEnteredMain(); }

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
};

QTEST_MAIN(TestFontGenerator)
#include "TestFontGenerator.moc"
