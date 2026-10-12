// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "TestClient.h"

#include "../src/client/InputHistory.h"
#include "../src/configuration/configuration.h"
#include "../src/display/GlInputPrompt.h"
#include "../src/display/GlTerminalBuffer.h"
#include "../src/mpi/RemoteEditDocumentOps.h"

#include <QTextCursor>
#include <QTextDocument>
#include <QtTest/QtTest>

TestClient::TestClient()
{
    setEnteredMain();
}

TestClient::~TestClient() = default;

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
    // Exercises adding "a" again: back() ("a", the oldest entry) equals the
    // new string, so it does NOT get re-added.
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

// --- GlTerminalBuffer & GlInputPrompt ---

void TestClient::glTerminalBufferAnsiParsing()
{
    mmgl::GlTerminalBuffer buf;
    buf.setColumnsRows(80, 24);
    buf.setMaxLines(10);

    buf.appendStreamText(QStringLiteral("Hello \033[1;31mWorld\033[0m\nLine 2\a"));

    QCOMPARE(buf.getLineCount(), static_cast<size_t>(2));
    QVERIFY(buf.takeBellTriggered());

    const auto &line0 = buf.getLine(0);
    QVERIFY(line0.cells.size() >= 11);
    QCOMPARE(static_cast<char16_t>(line0.cells[0].charCode), QChar::fromUcs2('H'));
    // "World" cells should have bold flag set
    QCOMPARE(static_cast<char16_t>(line0.cells[6].charCode), QChar::fromUcs2('W'));
    QVERIFY(line0.cells[6].bold);

    // Scrollback capping test
    for (int i = 0; i < 20; ++i) {
        buf.appendStreamText(QStringLiteral("Extra line\n"));
    }
    QCOMPARE(buf.getLineCount(), static_cast<size_t>(10));
}

void TestClient::glTerminalBufferSelectionAndCopy()
{
    mmgl::GlTerminalBuffer buf;
    buf.setColumnsRows(80, 24);
    buf.appendStreamText(QStringLiteral("First line\nSecond line"));

    buf.startSelection(0, 0);
    buf.updateSelection(4, 0);

    QVERIFY(buf.hasSelection());
    QVERIFY(buf.isCellSelected(0, 0));
    QVERIFY(buf.isCellSelected(4, 0));
    QVERIFY(!buf.isCellSelected(5, 0));

    QCOMPARE(buf.getSelectedText(), QStringLiteral("First"));
}

void TestClient::glInputPromptEditing()
{
    mmgl::GlInputPrompt prompt;
    QCOMPARE(prompt.getText(), QString());

    QKeyEvent keyA(QEvent::KeyPress, Qt::Key_A, Qt::NoModifier, QStringLiteral("a"));
    std::ignore = prompt.handleKeyPress(&keyA);
    QCOMPARE(prompt.getText(), QStringLiteral("a"));

    QKeyEvent keyB(QEvent::KeyPress, Qt::Key_B, Qt::NoModifier, QStringLiteral("b"));
    std::ignore = prompt.handleKeyPress(&keyB);
    QCOMPARE(prompt.getText(), QStringLiteral("ab"));

    QKeyEvent keyBk(QEvent::KeyPress, Qt::Key_Backspace, Qt::NoModifier);
    std::ignore = prompt.handleKeyPress(&keyBk);
    QCOMPARE(prompt.getText(), QStringLiteral("a"));

    // Enter to submit
    QKeyEvent keyEnter(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
    const auto optSubmitted = prompt.handleKeyPress(&keyEnter);
    QVERIFY(optSubmitted.has_value());
    QCOMPARE(*optSubmitted, QStringLiteral("a"));
    QCOMPARE(prompt.getText(), QString());

    // Password masking
    prompt.setEchoMode(EchoModeEnum::Hidden);
    std::ignore = prompt.handleKeyPress(&keyA);
    QCOMPARE(prompt.getDisplayText(), QStringLiteral("*"));
}

// --- RemoteEditDocumentOps ---
// Widget-free core of the MPI remote editor's text operations; exercised directly
// against a QTextDocument (no RemoteEditWidget/QPlainTextEdit involved).

void TestClient::remoteEditJustifyTextRewrapsLongLines()
{
    const QString input = QStringLiteral("one two three four five six seven eight nine ten\n");
    const QString result = mmqt::remoteEditJustifyText(input, 20);

    const QStringList lines = result.split(QChar('\n'), Qt::SkipEmptyParts);
    QVERIFY(lines.size() > 1);
    for (const QString &line : lines) {
        QVERIFY2(line.length() <= 20, qPrintable(line));
    }
    // All the original words must still be present, just rewrapped.
    for (const char *word :
         {"one", "two", "three", "four", "five", "six", "seven", "eight", "nine", "ten"}) {
        QVERIFY(result.contains(QString::fromLatin1(word)));
    }
}

void TestClient::remoteEditExpandTabsExpandsSelection()
{
    QTextDocument doc;
    doc.setPlainText(QStringLiteral("a\tb"));

    QTextCursor cur(&doc);
    cur.select(QTextCursor::Document);
    mmqt::remoteEditExpandTabs(cur);

    const QString text = doc.toPlainText();
    QVERIFY(!text.contains(QChar('\t')));
    // Tab stops are every 8 columns, so "a" (1 col) + tab expands to 7 spaces.
    QCOMPARE(text, QStringLiteral("a") + QString(7, QChar(' ')) + QStringLiteral("b"));
}

void TestClient::remoteEditRemoveDuplicateSpacesCollapses()
{
    QTextDocument doc;
    doc.setPlainText(QStringLiteral("a    b   c"));

    QTextCursor cur(&doc);
    cur.select(QTextCursor::Document);
    mmqt::remoteEditRemoveDuplicateSpaces(cur);

    QCOMPARE(doc.toPlainText(), QStringLiteral("a b c"));
}

void TestClient::remoteEditJoinLinesCombinesBlocks()
{
    QTextDocument doc;
    doc.setPlainText(QStringLiteral("line one\nline two"));

    // Only the first (and only non-final) block is selected; joinLines' "join the next
    // line if only one line is selected" feature should pull in the final block too.
    QTextCursor cur(&doc);
    cur.movePosition(QTextCursor::Start);
    cur.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
    mmqt::remoteEditJoinLines(cur);

    QCOMPARE(doc.toPlainText(), QStringLiteral("line one line two"));
}

void TestClient::remoteEditPrefixPartialSelectionQuotesLines()
{
    QTextDocument doc;
    doc.setPlainText(QStringLiteral("abc\ndef"));

    QTextCursor cur(&doc);
    cur.select(QTextCursor::Document);
    mmqt::remoteEditPrefixPartialSelection(cur, QStringLiteral("> "));

    QCOMPARE(doc.toPlainText(), QStringLiteral("> abc\n> def"));
}

void TestClient::remoteEditFindWrapsAround()
{
    QTextDocument doc;
    doc.setPlainText(QStringLiteral("foo bar foo"));

    // Position the cursor after the last "foo", so a forward search must wrap around
    // to find the first occurrence.
    QTextCursor cur(&doc);
    cur.movePosition(QTextCursor::End);

    const auto outcome = mmqt::remoteEditFind(doc, cur, QStringLiteral("foo"), {});
    QCOMPARE(outcome.result, mmqt::RemoteEditFindResultEnum::FOUND_AFTER_WRAP);
    QCOMPARE(outcome.cursor.selectedText(), QStringLiteral("foo"));
    QCOMPARE(outcome.cursor.selectionStart(), 0);

    // Searching for something absent must report NOT_FOUND and leave the returned
    // cursor equal to the one that was passed in.
    const auto missing = mmqt::remoteEditFind(doc, cur, QStringLiteral("nope"), {});
    QCOMPARE(missing.result, mmqt::RemoteEditFindResultEnum::NOT_FOUND);
}

void TestClient::remoteEditReplaceAllReplacesEveryOccurrence()
{
    QTextDocument doc;
    doc.setPlainText(QStringLiteral("foo bar foo baz foo"));

    const int replacements = mmqt::remoteEditReplaceAll(doc,
                                                        QStringLiteral("foo"),
                                                        QStringLiteral("qux"),
                                                        {});
    QCOMPARE(replacements, 3);
    QCOMPARE(doc.toPlainText(), QStringLiteral("qux bar qux baz qux"));
}

void TestClient::remoteEditStatusReportsTabsLongLinesAndTrailingSpace()
{
    QTextDocument doc;
    const QString longLine(85, QChar('x'));
    doc.setPlainText(QStringLiteral("a\tb\n") + longLine + QStringLiteral("\ntrailing   \n"));

    QTextCursor cur(&doc);
    cur.movePosition(QTextCursor::Start);
    const auto info = mmqt::computeRemoteEditStatus(cur);

    QCOMPARE(info.line, 1);
    QCOMPARE(info.col, 1);
    QVERIFY(!info.selection);
    QVERIFY(info.hasTabs);
    QVERIFY(info.hasLongLines);
    QVERIFY(info.hasTrailingSpace);
}

QTEST_MAIN(TestClient)

#include "TestClient.moc"
