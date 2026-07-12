// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "TestClient.h"

#include "../src/client/ClientLineModel.h"
#include "../src/configuration/configuration.h"

#include <QtTest/QtTest>

TestClient::TestClient()
{
    setEnteredMain();
}

TestClient::~TestClient() = default;

namespace { // anonymous
NODISCARD QString html(const ClientLineModel &model, const int row)
{
    return model.data(model.index(row, 0), static_cast<int>(ClientLineModel::RoleEnum::Html))
        .toString();
}
NODISCARD QString plain(const ClientLineModel &model, const int row)
{
    return model.data(model.index(row, 0), static_cast<int>(ClientLineModel::RoleEnum::Plain))
        .toString();
}
} // namespace

void TestClient::plainTextLine()
{
    ClientLineModel model;
    model.appendText(u"Hello world\n");

    // One finished line plus the always-present trailing partial row.
    QCOMPARE(model.rowCount(), 2);
    QCOMPARE(plain(model, 0), QStringLiteral("Hello world"));
    QCOMPARE(plain(model, 1), QString());
    // No ANSI styling was applied, so the html must have no <span> wrapper.
    QCOMPARE(html(model, 0), QStringLiteral("Hello world"));
}

void TestClient::foregroundColor()
{
    ClientLineModel model;
    model.appendText(u"\x1b[31mred text\x1b[0m\n");

    QCOMPARE(plain(model, 0), QStringLiteral("red text"));
    const QString h = html(model, 0);
    QVERIFY2(h.contains(QStringLiteral("color:#")), qPrintable(h));
    QVERIFY2(h.contains(QStringLiteral("red text")), qPrintable(h));
}

void TestClient::backgroundColor()
{
    ClientLineModel model;
    model.appendText(u"\x1b[37;46mtext\x1b[0m\n");

    const QString h = html(model, 0);
    QVERIFY2(h.contains(QStringLiteral("background-color:#")), qPrintable(h));
}

void TestClient::boldStyle()
{
    ClientLineModel model;
    model.appendText(u"\x1b[1mbold text\x1b[0m\n");

    const QString h = html(model, 0);
    QVERIFY2(h.contains(QStringLiteral("font-weight:bold")), qPrintable(h));
}

void TestClient::color256()
{
    ClientLineModel model;
    model.appendText(u"\x1b[38;5;196mtext\x1b[0m\n");

    const QString h = html(model, 0);
    QVERIFY2(h.contains(QStringLiteral("color:#")), qPrintable(h));
}

void TestClient::reverseVideo()
{
    ClientLineModel model;
    model.appendText(u"\x1b[7mtext\x1b[0m\n");

    const QString h = html(model, 0);
    // With no explicit fg/bg, reverse video still swaps the resolved default
    // colors, so both the foreground and background must differ from the
    // (unreversed) configured defaults.
    QVERIFY2(h.contains(QStringLiteral("color:#")), qPrintable(h));
    QVERIFY2(h.contains(QStringLiteral("background-color:#")), qPrintable(h));
}

void TestClient::partialLineAcrossCalls()
{
    ClientLineModel model;
    model.appendText(u"first ");
    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(plain(model, 0), QStringLiteral("first "));

    model.appendText(u"half\n");
    QCOMPARE(model.rowCount(), 2);
    QCOMPARE(plain(model, 0), QStringLiteral("first half"));
    QCOMPARE(plain(model, 1), QString());
}

void TestClient::scrollbackCap()
{
    const int savedLimit = getConfig().integratedClient.linesOfScrollback;
    setConfig().integratedClient.linesOfScrollback = 5;

    ClientLineModel model;
    for (int i = 0; i < 10; ++i) {
        model.appendText(QStringLiteral("line %1\n").arg(i));
    }

    // 5 finished lines (capped) + 1 trailing partial row.
    QCOMPARE(model.rowCount(), 6);
    // The oldest lines must have been trimmed from the front, keeping the
    // most recent ones.
    QCOMPARE(plain(model, 0), QStringLiteral("line 5"));
    QCOMPARE(plain(model, 4), QStringLiteral("line 9"));

    setConfig().integratedClient.linesOfScrollback = savedLimit;
}

void TestClient::clearResetsModel()
{
    ClientLineModel model;
    model.appendText(u"some text\nmore\n");
    QVERIFY(model.rowCount() > 1);

    model.clear();
    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(plain(model, 0), QString());
}

QTEST_MAIN(TestClient)

#include "TestClient.moc"
