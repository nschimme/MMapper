// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2023 The MMapper Authors

#include "TestRemoteEditWidget.h"
#include "../src/mpi/remoteeditwidget.h" // Adjust path as necessary

#include <QtTest/QtTest>
#include <QPlainTextEdit> // For QTextDocument::FindFlags

// Helper to create a RemoteTextEdit instance for testing
static RemoteTextEdit* createEditor(const QString& initialText) {
    // We don't have a full RemoteEditWidget, so we create RemoteTextEdit directly.
    // This means some features of RemoteEditWidget (like status bar) won't be tested here.
    auto* editor = new RemoteTextEdit(initialText, nullptr);
    // Set a default font for consistent metrics if necessary, though not strictly needed for these tests
    QFont font("Monospace");
    font.setStyleHint(QFont::TypeWriter);
    editor->setFont(font);
    return editor;
}

void TestRemoteEditWidget::initTestCase()
{
    // Initialization before all tests
}

void TestRemoteEditWidget::cleanupTestCase()
{
    // Cleanup after all tests
}

void TestRemoteEditWidget::testFindNext()
{
    RemoteTextEdit* editor = createEditor("Hello world\nSecond line world\nThird world");
    QTextDocument::FindFlags flags;

    // Simple find
    QVERIFY(editor->findNext("world", flags));
    QCOMPARE(editor->textCursor().selectedText(), "world");
    QCOMPARE(editor->textCursor().blockNumber(), 0);

    // Find next
    QVERIFY(editor->findNext("world", flags));
    QCOMPARE(editor->textCursor().selectedText(), "world");
    QCOMPARE(editor->textCursor().blockNumber(), 1);

    // Find last
    QVERIFY(editor->findNext("world", flags));
    QCOMPARE(editor->textCursor().selectedText(), "world");
    QCOMPARE(editor->textCursor().blockNumber(), 2);

    // Not found after last
    QVERIFY(!editor->findNext("world", flags));

    // Case sensitive
    editor->moveCursor(QTextCursor::Start);
    flags |= QTextDocument::FindCaseSensitively;
    QVERIFY(editor->findNext("Hello", flags));
    QVERIFY(!editor->findNext("hello", flags)); // Should not find lowercase

    delete editor;
}

void TestRemoteEditWidget::testFindPrevious()
{
    RemoteTextEdit* editor = createEditor("Hello world\nSecond line world\nThird world");
    QTextDocument::FindFlags flags;

    editor->moveCursor(QTextCursor::End);

    // Simple find previous
    QVERIFY(editor->findPrevious("world", flags));
    QCOMPARE(editor->textCursor().selectedText(), "world");
    QCOMPARE(editor->textCursor().blockNumber(), 2);

    // Find previous again
    QVERIFY(editor->findPrevious("world", flags));
    QCOMPARE(editor->textCursor().selectedText(), "world");
    QCOMPARE(editor->textCursor().blockNumber(), 1);

    // Find first
    QVERIFY(editor->findPrevious("world", flags));
    QCOMPARE(editor->textCursor().selectedText(), "world");
    QCOMPARE(editor->textCursor().blockNumber(), 0);

    // Not found before first
    QVERIFY(!editor->findPrevious("world", flags));

    delete editor;
}

void TestRemoteEditWidget::testReplace()
{
    RemoteTextEdit* editor = createEditor("Replace this word. And this word too.");
    QTextDocument::FindFlags flags;

    QVERIFY(editor->findNext("word", flags)); // Find first "word"
    editor->replace("word", "text", flags); // Replace it and find next
    QCOMPARE(editor->document()->toPlainText(), "Replace this text. And this word too.");
    QVERIFY(editor->textCursor().hasSelection());
    QCOMPARE(editor->textCursor().selectedText(), "word"); // Next "word" should be selected

    editor->replace("word", "text", flags); // Replace second "word"
    QCOMPARE(editor->document()->toPlainText(), "Replace this text. And this text too.");
    QVERIFY(!editor->textCursor().hasSelection()); // No more "word"s to select

    delete editor;
}

void TestRemoteEditWidget::testReplaceAll()
{
    RemoteTextEdit* editor = createEditor("Replace all words. All words. Yes, all words.");
    QTextDocument::FindFlags flags;

    editor->replaceAll("words", "strings", flags);
    QCOMPARE(editor->document()->toPlainText(), "Replace all strings. All strings. Yes, all strings.");

    // Test with no occurrences
    editor->replaceAll("nonexistent", "something", flags);
    QCOMPARE(editor->document()->toPlainText(), "Replace all strings. All strings. Yes, all strings.");

    delete editor;
}

void TestRemoteEditWidget::testGotoLine()
{
    RemoteTextEdit* editor = createEditor("Line 1\nLine 2\nLine 3\nLine 4");

    editor->gotoLine(2); // Go to 3rd line (0-indexed)
    QCOMPARE(editor->textCursor().blockNumber(), 2);
    QVERIFY(editor->textCursor().atBlockStart());

    editor->gotoLine(0); // Go to 1st line
    QCOMPARE(editor->textCursor().blockNumber(), 0);

    // Test going to last line
    editor->gotoLine(editor->document()->blockCount() - 1);
    QCOMPARE(editor->textCursor().blockNumber(), 3);

    delete editor;
}

void TestRemoteEditWidget::testFindWrapAround()
{
    RemoteTextEdit* editor = createEditor("Find me here.\nAnd also find me here.");
    QTextDocument::FindFlags flags;

    // Find "me" - first occurrence
    QVERIFY(editor->findNext("me", flags));
    QCOMPARE(editor->textCursor().positionInBlock(), 7); // "Find |me| here."

    // Find "me" again - second occurrence
    QVERIFY(editor->findNext("me", flags));
    QCOMPARE(editor->textCursor().blockNumber(), 1);
    QCOMPARE(editor->textCursor().positionInBlock(), 16); // "And also find |me| here."

    // Try to find "me" again - should fail without wrap
    QVERIFY(!editor->findNext("me", flags));

    // Now, with wrap around (simulated by moving cursor to start, as RemoteEditWidget does)
    // This specific test is more for the logic in RemoteEditWidget::slot_findNext,
    // but we test the underlying findNext behavior from start.
    editor->moveCursor(QTextCursor::Start);
    QVERIFY(editor->findNext("me", flags)); // Should find the first one again
    QCOMPARE(editor->textCursor().positionInBlock(), 7);

    delete editor;
}

// QTEST_MAIN(TestRemoteEditWidget) // This would make this file a standalone test executable.
// For integration into a larger test suite, you typically don't use QTEST_MAIN here.
// Instead, you'd have a main test runner that includes all test objects.
// For now, let's assume it will be part of a larger suite.
// If running standalone, uncomment QTEST_MAIN and adjust CMakeLists.txt

// To make this runnable with the existing test infrastructure, we'd need to:
// 1. Add this file to CMakeLists.txt in the tests/ directory.
// 2. Ensure the main test runner (if one exists) instantiates and runs TestRemoteEditWidget.
// For now, this provides the test code.
// The CMakeLists.txt part:
// add_executable(TestRemoteEditWidget TestRemoteEditWidget.cpp)
// target_link_libraries(TestRemoteEditWidget Qt${QT_VERSION_MAJOR}::Test YourAppLib) # YourAppLib should include RemoteEditWidget sources
// add_test(NAME TestRemoteEditWidget COMMAND TestRemoteEditWidget)

// A simplified main for running just these tests if needed for quick verification:
/*
#include <QApplication>
int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    TestRemoteEditWidget tc;
    return QTest::qExec(&tc, argc, argv);
}
*/
