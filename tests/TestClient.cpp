// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "TestClient.h"

#include "../src/client/ClientController.h"
#include "../src/client/ClientLineModel.h"
#include "../src/client/HotkeyManager.h"
#include "../src/client/InputHistory.h"
#include "../src/configuration/configuration.h"

#include <memory>

#include <QSignalSpy>
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

namespace { // anonymous

// Local replicas of InputWidget::forwardHistory()/backwardHistory() (see
// inputwidget.cpp), operating directly on an InputHistory instance instead
// of a QPlainTextEdit, so the iterator dance (including its boundary
// asymmetry) can be pinned without constructing a QWidget.
struct NODISCARD HistoryNav final
{
    InputHistory &history;
    QString text;
    bool boundaryHit = false;
    QString boundaryMessage;

    explicit HistoryNav(InputHistory &history_)
        : history(history_)
    {}

    // Mirrors InputWidget::backwardHistory() (Up key).
    void up()
    {
        boundaryHit = false;
        if (history.atEnd()) {
            boundaryHit = true;
            boundaryMessage = "Reached end of input history";
            return;
        }
        text = history.value();
        if (!history.atEnd()) {
            history.forward();
        }
    }

    // Mirrors InputWidget::forwardHistory() (Down key).
    void down()
    {
        boundaryHit = false;
        text.clear();
        if (history.atFront()) {
            boundaryHit = true;
            boundaryMessage = "Reached beginning of input history";
            return;
        }
        if (history.atEnd()) {
            history.backward();
        }
        if (!history.atFront()) {
            history.backward();
            text = history.value();
        }
    }
};

} // namespace

void TestClient::inputHistoryDedupVsBackOnly()
{
    // InputHistory::addInputLine() only dedups against back() (the OLDEST
    // entry, since push_front() is used to add the newest), not against the
    // most-recently-added entry. This is a deliberate pin of that quirk.
    InputHistory history;
    history.addInputLine("a");
    history.addInputLine("a"); // back() == "a" -> not added again
    HistoryNav nav(history);
    nav.up();
    QCOMPARE(nav.text, QStringLiteral("a"));
    nav.up();
    QVERIFY(nav.boundaryHit); // only one entry total

    InputHistory history2;
    history2.addInputLine("a");
    history2.addInputLine("b");
    // back() is "a" (oldest), which != "b", so "b" is NOT deduped against
    // the front-most "b"... rather this exercises adding "a" again, whose
    // back() ("a") equals the new string, so it does NOT get re-added.
    history2.addInputLine("a");
    HistoryNav nav2(history2);
    nav2.up();
    QCOMPARE(nav2.text, QStringLiteral("b"));
    nav2.up();
    QCOMPARE(nav2.text, QStringLiteral("a"));
    nav2.up();
    QVERIFY(nav2.boundaryHit); // only two entries total: [b, a]
}

void TestClient::inputHistoryEmptyLineSkipped()
{
    InputHistory history;
    history.addInputLine(QString());
    HistoryNav nav(history);
    nav.up();
    QVERIFY(nav.boundaryHit);
}

void TestClient::inputHistoryCap()
{
    const int savedLimit = getConfig().integratedClient.linesOfInputHistory;
    setConfig().integratedClient.linesOfInputHistory = 2;

    InputHistory history;
    history.addInputLine("one");
    history.addInputLine("two");
    history.addInputLine("three");

    HistoryNav nav(history);
    nav.up();
    QCOMPARE(nav.text, QStringLiteral("three"));
    nav.up();
    QCOMPARE(nav.text, QStringLiteral("two"));
    nav.up();
    QVERIFY(nav.boundaryHit); // "one" was trimmed by the cap

    setConfig().integratedClient.linesOfInputHistory = savedLimit;
}

void TestClient::inputHistoryNavigation()
{
    const int savedLimit = getConfig().integratedClient.linesOfInputHistory;
    setConfig().integratedClient.linesOfInputHistory = 100;

    InputHistory history;
    history.addInputLine("cmd1");
    history.addInputLine("cmd2");
    history.addInputLine("cmd3");

    HistoryNav nav(history);
    nav.up();
    QCOMPARE(nav.text, QStringLiteral("cmd3"));
    nav.up();
    QCOMPARE(nav.text, QStringLiteral("cmd2"));
    nav.up();
    QCOMPARE(nav.text, QStringLiteral("cmd1"));
    nav.up();
    QVERIFY(nav.boundaryHit);
    QCOMPARE(nav.boundaryMessage, QStringLiteral("Reached end of input history"));
    // Text is left untouched (still the last retrieved value) on this boundary.
    QCOMPARE(nav.text, QStringLiteral("cmd1"));

    // Pin the down-navigation asymmetry: the first Down after running off
    // the end skips over "cmd1" (the value last shown by up()) and lands on
    // "cmd2" instead.
    nav.down();
    QVERIFY(!nav.boundaryHit);
    QCOMPARE(nav.text, QStringLiteral("cmd2"));
    nav.down();
    QCOMPARE(nav.text, QStringLiteral("cmd3"));
    nav.down();
    QVERIFY(nav.boundaryHit);
    QCOMPARE(nav.boundaryMessage, QStringLiteral("Reached beginning of input history"));
    // Text is cleared on this boundary.
    QCOMPARE(nav.text, QString());

    setConfig().integratedClient.linesOfInputHistory = savedLimit;
}

void TestClient::tabHistoryWordExtraction()
{
    const int savedCap = getConfig().integratedClient.tabCompletionDictionarySize;
    setConfig().integratedClient.tabCompletionDictionarySize = 100;

    TabHistory history;
    // MIN_WORD_LENGTH is 3, and the extraction requires length > 3, so only
    // words of 4+ characters are kept ("cat" and "at" are dropped, "look"
    // and "kill" are kept). Newest word is pushed to the front, so the
    // dictionary order is [kill, look].
    history.addInputLine("look at cat and kill");

    // "kill" is returned normally; "look" is the last (oldest) dictionary
    // entry, so finding it reverts to nullopt per nextMatch()'s wraparound
    // contract (see InputHistory.h), and the iterator resets.
    QCOMPARE(history.nextMatch(""), std::make_optional(QStringLiteral("kill")));
    QCOMPARE(history.nextMatch(""), std::optional<QString>{});
    QCOMPARE(history.nextMatch(""), std::make_optional(QStringLiteral("kill")));

    setConfig().integratedClient.tabCompletionDictionarySize = savedCap;
}

void TestClient::tabHistoryCap()
{
    const int savedCap = getConfig().integratedClient.tabCompletionDictionarySize;
    setConfig().integratedClient.tabCompletionDictionarySize = 2;

    TabHistory history;
    history.addInputLine("alpha");
    history.addInputLine("bravo");
    history.addInputLine("gamma");

    // Only the two most-recently-added words survive the cap: [gamma, bravo].
    // "bravo" is the last (oldest) entry, so per nextMatch()'s wraparound
    // contract it reverts to nullopt instead of being returned directly.
    QCOMPARE(history.nextMatch(""), std::make_optional(QStringLiteral("gamma")));
    QCOMPARE(history.nextMatch(""), std::optional<QString>{});

    setConfig().integratedClient.tabCompletionDictionarySize = savedCap;
}

void TestClient::tabHistoryNextMatchCycling()
{
    const int savedCap = getConfig().integratedClient.tabCompletionDictionarySize;
    setConfig().integratedClient.tabCompletionDictionarySize = 100;

    TabHistory history;
    history.addInputLine("apple avocado banana apricot");
    // Dictionary (newest first): apricot, banana, avocado, apple. Only
    // "apricot" and "apple" start with "ap" ("avocado" starts with "av").

    QCOMPARE(history.nextMatch("ap"), std::make_optional(QStringLiteral("apricot")));
    // "apple" is the next match, but it is also the last overall dictionary
    // entry: per TabHistory::nextMatch()'s documented wraparound contract,
    // finding a match on the final entry reverts to nullopt (mirroring
    // InputWidget::tabComplete()'s apply-then-immediately-revert quirk for
    // that case) and resets the iterator, skipping over "banana" and
    // "avocado" along the way without returning them (they don't match).
    QCOMPARE(history.nextMatch("ap"), std::optional<QString>{});
    // The iterator was reset, so the cycle restarts from the newest entry.
    QCOMPARE(history.nextMatch("ap"), std::make_optional(QStringLiteral("apricot")));

    // No match at all also yields nullopt and resets.
    QCOMPARE(history.nextMatch("zzz"), std::optional<QString>{});
    QCOMPARE(history.nextMatch("ap"), std::make_optional(QStringLiteral("apricot")));

    setConfig().integratedClient.tabCompletionDictionarySize = savedCap;
}

namespace { // anonymous

// Records every call made through the ClientControllerBackend seam so tests
// can assert on them without a real ClientTelnet/socket.
struct NODISCARD FakeBackend final : public ClientControllerBackend
{
    bool connected = false;
    int connectCount = 0;
    int disconnectCount = 0;
    QStringList sentToMud;
    QList<QPair<int, int>> windowSizes;

private:
    void virt_connectToMud() final { ++connectCount; }
    void virt_disconnectFromMud() final { ++disconnectCount; }
    NODISCARD bool virt_isConnected() const final { return connected; }
    void virt_sendToMud(const QString &data) final { sentToMud.push_back(data); }
    void virt_windowSizeChanged(const int width, const int height) final
    {
        windowSizes.push_back({width, height});
    }
};

} // namespace

void TestClient::controllerSplitCommands()
{
    const bool savedUse = getConfig().integratedClient.useCommandSeparator;
    const QString savedSep = getConfig().integratedClient.commandSeparator;

    setConfig().integratedClient.useCommandSeparator = true;
    setConfig().integratedClient.commandSeparator = QStringLiteral(";;");

    QCOMPARE(ClientController::splitCommands(QStringLiteral("l;;look")),
             QStringList({QStringLiteral("l"), QStringLiteral("look")}));

    // A backslash-escaped separator is not split on, and the backslash is
    // stripped from the (single) resulting command.
    QCOMPARE(ClientController::splitCommands(QStringLiteral("l\\;;look")),
             QStringList({QStringLiteral("l;;look")}));

    setConfig().integratedClient.useCommandSeparator = false;
    QCOMPARE(ClientController::splitCommands(QStringLiteral("l;;look")),
             QStringList({QStringLiteral("l;;look")}));

    setConfig().integratedClient.useCommandSeparator = savedUse;
    setConfig().integratedClient.commandSeparator = savedSep;
}

void TestClient::controllerSendInputDisconnected()
{
    ClientLineModel model;
    HotkeyManager hotkeys;
    ClientController controller(model, hotkeys, nullptr);
    auto backend = std::make_unique<FakeBackend>();
    FakeBackend &fake = *backend;
    controller.setBackend(std::move(backend));

    controller.sendInput(QStringLiteral("look"));

    // Disconnected: exactly one connect attempt, and the command is
    // DROPPED, not queued (parity with ClientWidget.cpp's
    // virt_sendUserInput()).
    QCOMPARE(fake.connectCount, 1);
    QVERIFY(fake.sentToMud.isEmpty());
    // Local echo is still shown regardless.
    QVERIFY2(model.toPlainText().contains(QStringLiteral("look")), qPrintable(model.toPlainText()));
}

void TestClient::controllerSendInputConnected()
{
    ClientLineModel model;
    HotkeyManager hotkeys;
    ClientController controller(model, hotkeys, nullptr);
    auto backend = std::make_unique<FakeBackend>();
    FakeBackend &fake = *backend;
    fake.connected = true;
    controller.setBackend(std::move(backend));

    controller.sendInput(QStringLiteral("look"));

    QCOMPARE(fake.connectCount, 0);
    QCOMPARE(fake.sentToMud, QStringList{QStringLiteral("look\n")});
    QVERIFY2(model.toPlainText().contains(QStringLiteral("look")), qPrintable(model.toPlainText()));

    // The raw line was recorded in the input history.
    const QVariant up = controller.historyUp();
    QCOMPARE(up.toString(), QStringLiteral("look"));
}

void TestClient::controllerSendPassword()
{
    ClientLineModel model;
    HotkeyManager hotkeys;
    ClientController controller(model, hotkeys, nullptr);
    auto backend = std::make_unique<FakeBackend>();
    FakeBackend &fake = *backend;
    fake.connected = true;
    controller.setBackend(std::move(backend));

    controller.sendPassword(QStringLiteral("secret"));

    QCOMPARE(fake.sentToMud, QStringList{QStringLiteral("secret\n")});
    QVERIFY2(model.toPlainText().contains(QStringLiteral("******")),
             qPrintable(model.toPlainText()));
    // sendPassword() does not itself change echo mode.
    QCOMPARE(controller.getEchoVisible(), true);
}

void TestClient::controllerEchoModeChanged()
{
    ClientLineModel model;
    HotkeyManager hotkeys;
    ClientController controller(model, hotkeys, nullptr);

    QSignalSpy spyEcho(&controller, &ClientController::echoVisibleChanged);
    QSignalSpy spyPrompt(&controller, &ClientController::sig_passwordPromptRequested);

    controller.onEchoModeChanged(false);

    QCOMPARE(spyEcho.count(), 1);
    QCOMPARE(spyPrompt.count(), 1);
    QCOMPARE(controller.getEchoVisible(), false);
}

void TestClient::controllerSendToUserWhileHidden()
{
    ClientLineModel model;
    HotkeyManager hotkeys;
    ClientController controller(model, hotkeys, nullptr);
    controller.onEchoModeChanged(false);

    QSignalSpy spyPrompt(&controller, &ClientController::sig_passwordPromptRequested);
    controller.onSendToUser(QStringLiteral("Password: "));

    QCOMPARE(spyPrompt.count(), 1);
    QVERIFY2(model.toPlainText().contains(QStringLiteral("Password: ")),
             qPrintable(model.toPlainText()));
}

void TestClient::controllerDisconnectedReconnectHint()
{
    ClientLineModel model;
    HotkeyManager hotkeys;
    ClientController controller(model, hotkeys, nullptr);

    controller.onDisconnected();

    QVERIFY2(model.toPlainText().contains(QStringLiteral("Press return to reconnect.")),
             qPrintable(model.toPlainText()));
}

void TestClient::controllerHotkeys()
{
    ClientLineModel model;
    HotkeyManager hotkeys;
    hotkeys.resetToDefaults();
    ClientController controller(model, hotkeys, nullptr);
    auto backend = std::make_unique<FakeBackend>();
    FakeBackend &fake = *backend;
    fake.connected = true;
    controller.setBackend(std::move(backend));

    QVERIFY(controller.isHotkey(Qt::Key_F1, Qt::NoModifier));
    QVERIFY(controller.sendHotkey(Qt::Key_F1, Qt::NoModifier));
    QCOMPARE(fake.sentToMud, QStringList{QStringLiteral("F1\n")});

    // Hotkeys never touch the input history.
    const QVariant up = controller.historyUp();
    QVERIFY(!up.isValid());

    hotkeys.clear();
}

QTEST_MAIN(TestClient)

#include "TestClient.moc"
