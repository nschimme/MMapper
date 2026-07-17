// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "RemoteEditController.h"

#include "../configuration/configuration.h"
#include "../global/AnsiTextUtils.h"
#include "../global/Consts.h"
#include "../global/TabUtils.h"
#include "../global/TextUtils.h"
#include "../viewers/AnsiViewWindow.h"
#include "RemoteEditDocumentOps.h"
#include "RemoteEditHighlighter.h"

#include <utility>

#include <QFont>
#include <QQuickTextDocument>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextOption>

using namespace char_consts;

namespace { // anonymous

// Builds a forward selection cursor (anchor at selectionStart, position at
// selectionEnd) on doc; used by every op that only needs "the selected
// range" and doesn't care which end is the caret (matches how
// RemoteEditDocumentOps.h's functions consume a cursor via
// selectionStart()/selectionEnd(), which are always min/max regardless of
// direction).
NODISCARD QTextCursor makeSelectionCursor(QTextDocument *const doc,
                                          const int selectionStart,
                                          const int selectionEnd)
{
    QTextCursor cur(doc);
    cur.setPosition(selectionStart);
    cur.setPosition(selectionEnd, QTextCursor::KeepAnchor);
    return cur;
}

} // namespace

RemoteEditController::RemoteEditController(const bool editSession,
                                           QString title,
                                           QString body,
                                           QObject *const parent)
    : QObject(parent)
    , m_editSession(editSession)
    , m_title(std::move(title))
    , m_body(std::move(body))
{
    QFont font;
    font.fromString(getConfig().integratedClient.font);
    m_fontFamily = font.family();
    m_fontPointSize = font.pointSize();
}

RemoteEditController::~RemoteEditController() = default;

void RemoteEditController::attachDocument(QQuickTextDocument *const doc)
{
    if (doc == nullptr) {
        return;
    }
    QTextDocument *const document = doc->textDocument();
    if (document == nullptr || document == m_document) {
        return;
    }

    m_document = document;
    m_highlighter = new RemoteEditHighlighter(m_document, mmqt::REMOTE_EDIT_MAX_LENGTH);

    // Matches RemoteTextEdit::showWhitespace(false): whitespace glyphs start
    // hidden, but AddSpaceForLineAndParagraphSeparators is always set.
    QTextOption option = m_document->defaultTextOption();
    option.setFlags(option.flags() | QTextOption::AddSpaceForLineAndParagraphSeparators);
    m_document->setDefaultTextOption(option);

    connect(m_document,
            &QTextDocument::contentsChanged,
            this,
            &RemoteEditController::recomputeDirty);

    recomputeDirty();
}

void RemoteEditController::recomputeDirty()
{
    if (m_document == nullptr) {
        return;
    }
    const bool newDirty = QString::compare(m_document->toPlainText(), m_body, Qt::CaseSensitive)
                          != 0;
    if (newDirty != m_dirty) {
        m_dirty = newDirty;
        emit sig_dirtyChanged();
    }
}

void RemoteEditController::updateStatus(const int selectionStart,
                                        const int selectionEnd,
                                        const int cursorPosition)
{
    if (m_document == nullptr) {
        return;
    }

    QTextCursor cur(m_document);
    if (selectionStart == selectionEnd) {
        cur.setPosition(cursorPosition);
    } else {
        const int anchor = (cursorPosition == selectionStart) ? selectionEnd : selectionStart;
        cur.setPosition(anchor);
        cur.setPosition(cursorPosition, QTextCursor::KeepAnchor);
    }

    const auto info = mmqt::computeRemoteEditStatus(cur);

    QString status = QString("Line %1, Column %2").arg(info.line).arg(info.col);
    if (info.nonAnsiAwareCol) {
        status.append(QString(" (non-ansi-aware: %1)").arg(*info.nonAnsiAwareCol));
    }

    if (info.selection) {
        const auto plural = [](auto n) { return (n == 1) ? "" : "s"; };
        status.append(QString(", Selection: %1 char%2 on %3 line%4")
                          .arg(info.selection->chars)
                          .arg(plural(info.selection->chars))
                          .arg(info.selection->lines)
                          .arg(plural(info.selection->lines)));
    } else if (!info.ansiCode.isEmpty()) {
        status.append(QString(", AnsiCode: {%1}").arg(info.ansiCode));
    }

    const bool hasSelection = info.selection.has_value();
    bool first = true;
    const auto err = [&first, &hasSelection, &status](auto &&x) {
        if (first) {
            status.append(hasSelection ? ", Selected-Lines-Err: " : ", Document-Err: ");
            first = false;
        } else {
            status.append(", ");
        }
        status.append(std::forward<decltype(x)>(x));
    };

    if (info.hasTabs) {
        err("Tabs");
    }
    if (info.hasTrailingSpace) {
        err("Trailing-Spaces");
    }
    if (info.hasLongLines) {
        err("Long-lines");
    }
    if (first) {
        status.append(hasSelection ? ", Selected-Lines: Ok" : ", Document: Ok");
    }

    if (status != m_statusText) {
        m_statusText = status;
        emit sig_statusChanged();
    }
}

void RemoteEditController::justifyAll()
{
    if (m_document == nullptr) {
        return;
    }
    const QString old = m_document->toPlainText();
    QTextCursor cur(m_document);
    cur.select(QTextCursor::Document);
    cur.insertText(mmqt::remoteEditJustifyText(old, mmqt::REMOTE_EDIT_MAX_LENGTH));
}

void RemoteEditController::justifySelection(const int selectionStart, const int selectionEnd)
{
    if (m_document == nullptr) {
        return;
    }
    mmqt::remoteEditJustifyLines(makeSelectionCursor(m_document, selectionStart, selectionEnd),
                                 mmqt::REMOTE_EDIT_MAX_LENGTH);
}

void RemoteEditController::expandTabs(const int selectionStart, const int selectionEnd)
{
    if (m_document == nullptr) {
        return;
    }
    mmqt::remoteEditExpandTabs(makeSelectionCursor(m_document, selectionStart, selectionEnd));
}

void RemoteEditController::removeTrailingWhitespace(const int selectionStart, const int selectionEnd)
{
    if (m_document == nullptr) {
        return;
    }
    mmqt::remoteEditRemoveTrailingWhitespace(
        makeSelectionCursor(m_document, selectionStart, selectionEnd));
}

void RemoteEditController::removeDuplicateSpaces(const int selectionStart, const int selectionEnd)
{
    if (m_document == nullptr) {
        return;
    }
    mmqt::remoteEditRemoveDuplicateSpaces(
        makeSelectionCursor(m_document, selectionStart, selectionEnd));
}

void RemoteEditController::normalizeAnsi()
{
    if (m_document == nullptr) {
        return;
    }
    const QString old = m_document->toPlainText();
    if (auto output = mmqt::remoteEditNormalizeAnsi(old)) {
        QTextCursor cur(m_document);
        cur.select(QTextCursor::Document);
        cur.insertText(*output);
    }
}

int RemoteEditController::insertAnsiReset(const int cursorPosition)
{
    if (m_document == nullptr) {
        return cursorPosition;
    }
    QTextCursor cur(m_document);
    cur.setPosition(cursorPosition);
    const auto reset = AnsiString::get_reset_string();
    cur.insertText(QString::fromUtf8(reset.c_str()));
    return cur.position();
}

void RemoteEditController::joinLines(const int selectionStart, const int selectionEnd)
{
    if (m_document == nullptr) {
        return;
    }
    mmqt::remoteEditJoinLines(makeSelectionCursor(m_document, selectionStart, selectionEnd));
}

void RemoteEditController::quoteLines(const int selectionStart, const int selectionEnd)
{
    if (m_document == nullptr) {
        return;
    }
    mmqt::remoteEditPrefixPartialSelection(makeSelectionCursor(m_document,
                                                               selectionStart,
                                                               selectionEnd),
                                           "> ");
}

void RemoteEditController::indentSelection(const int selectionStart, const int selectionEnd)
{
    if (m_document == nullptr) {
        return;
    }
    mmqt::remoteEditPrefixPartialSelection(makeSelectionCursor(m_document,
                                                               selectionStart,
                                                               selectionEnd),
                                           string_consts::S_TWO_SPACES);
}

int RemoteEditController::insertTabExpanded(const int cursorPosition)
{
    if (m_document == nullptr) {
        return cursorPosition;
    }

    QTextCursor cur(m_document);
    cur.setPosition(cursorPosition);

    mmqt::RemoteEditUndoGroup raii(cur);
    cur.insertText(string_consts::S_TAB);

    const int col_before = cur.positionInBlock();
    const auto &block = cur.block();
    const QString &s = block.text().left(col_before);
    const int col_after = mmqt::measureExpandedTabsOneLine(s, 0);
    mmqt::remoteEditExpandTabsOnLine(cur);

    return block.position() + col_after;
}

void RemoteEditController::unindentSelection(const int selectionStart, const int selectionEnd)
{
    if (m_document == nullptr) {
        return;
    }
    mmqt::remoteEditUnindentPartialSelection(
        makeSelectionCursor(m_document, selectionStart, selectionEnd));
}

void RemoteEditController::toggleWhitespace()
{
    if (m_document == nullptr) {
        return;
    }
    m_showingWhitespace = !m_showingWhitespace;

    QTextOption option = m_document->defaultTextOption();
    if (m_showingWhitespace) {
        option.setFlags(option.flags() | QTextOption::ShowTabsAndSpaces);
    } else {
        option.setFlags(option.flags() & ~QTextOption::ShowTabsAndSpaces);
    }
    option.setFlags(option.flags() | QTextOption::AddSpaceForLineAndParagraphSeparators);
    m_document->setDefaultTextOption(option);

    emit sig_showingWhitespaceChanged();
}

void RemoteEditController::previewAnsi()
{
    if (m_document == nullptr) {
        return;
    }
    QString s = m_document->toPlainText();
    if (!s.endsWith(C_NEWLINE)) {
        s += C_NEWLINE;
    }
    m_preview = makeAnsiViewWindow("MMapper Editor Preview", m_title, mmqt::toStdStringUtf8(s));
}

QVariantMap RemoteEditController::find(const QString &term,
                                       const bool backwards,
                                       const int selectionStart,
                                       const int selectionEnd)
{
    QVariantMap result{{"found", false}, {"start", -1}, {"end", -1}, {"message", QString()}};
    if (m_document == nullptr || term.isEmpty()) {
        return result;
    }

    QTextCursor cur = makeSelectionCursor(m_document, selectionStart, selectionEnd);
    QTextDocument::FindFlags flags;
    if (backwards) {
        flags |= QTextDocument::FindBackward;
    }

    const auto outcome = mmqt::remoteEditFind(*m_document, cur, term, flags);
    switch (outcome.result) {
    case mmqt::RemoteEditFindResultEnum::FOUND:
        result["found"] = true;
        result["start"] = outcome.cursor.selectionStart();
        result["end"] = outcome.cursor.selectionEnd();
        result["message"] = QString("Found: '%1'").arg(term);
        break;
    case mmqt::RemoteEditFindResultEnum::FOUND_AFTER_WRAP:
        result["found"] = true;
        result["start"] = outcome.cursor.selectionStart();
        result["end"] = outcome.cursor.selectionEnd();
        result["message"] = QString("Found (from %1): '%2'").arg(backwards ? "end" : "top").arg(term);
        break;
    case mmqt::RemoteEditFindResultEnum::NOT_FOUND:
        result["message"] = QString("Not found: '%1'").arg(term);
        break;
    }
    return result;
}

QVariantMap RemoteEditController::replaceCurrent(const QString &findTerm,
                                                 const QString &replaceTerm,
                                                 const int selectionStart,
                                                 const int selectionEnd)
{
    QVariantMap result{{"replaced", false},
                       {"found", false},
                       {"start", -1},
                       {"end", -1},
                       {"message", QString()}};
    if (m_document == nullptr || findTerm.isEmpty()) {
        return result;
    }

    QTextCursor cur = makeSelectionCursor(m_document, selectionStart, selectionEnd);
    const bool replaced = mmqt::remoteEditReplaceCurrentIfMatches(cur,
                                                                  findTerm,
                                                                  replaceTerm,
                                                                  QTextDocument::FindFlags());
    result["replaced"] = replaced;

    // Always find the next occurrence after attempting replacement, matching
    // RemoteEditWidget::slot_handleReplaceCurrentRequested(); its returned
    // "message" is what a real status bar would show last (see this class's
    // doc comment).
    const QVariantMap findResult = find(findTerm, false, cur.selectionStart(), cur.selectionEnd());
    result["found"] = findResult.value("found");
    result["start"] = findResult.value("start");
    result["end"] = findResult.value("end");
    result["message"] = findResult.value("message");
    return result;
}

QVariantMap RemoteEditController::replaceAll(const QString &findTerm, const QString &replaceTerm)
{
    QVariantMap result{{"count", 0}, {"message", QString()}};
    if (m_document == nullptr || findTerm.isEmpty()) {
        return result;
    }

    const int replacements = mmqt::remoteEditReplaceAll(*m_document,
                                                        findTerm,
                                                        replaceTerm,
                                                        QTextDocument::FindFlags());
    result["count"] = replacements;
    if (replacements > 0) {
        result["message"]
            = QString("Replaced %1 occurrence(s) of '%2'.").arg(replacements).arg(findTerm);
    } else {
        result["message"] = QString("Not found for replace all: '%1'.").arg(findTerm);
    }
    return result;
}

QVariantMap RemoteEditController::gotoLine(const int lineNum)
{
    QVariantMap result{{"found", false}, {"start", -1}, {"message", QString()}};
    if (m_document == nullptr) {
        return result;
    }

    if (auto cursor = mmqt::remoteEditGotoLine(*m_document, lineNum)) {
        result["found"] = true;
        result["start"] = cursor->position();
        result["message"] = QString("Moved to line %1").arg(lineNum);
    } else {
        result["message"] = QString("Error: Line %1 not found.").arg(lineNum);
    }
    return result;
}

void RemoteEditController::submit()
{
    if (m_submitted) {
        return;
    }
    m_submitted = true;
    emit sig_submittedChanged();
    emit sig_save(m_document != nullptr ? m_document->toPlainText() : m_body);
}

void RemoteEditController::cancel()
{
    if (m_submitted) {
        return;
    }
    m_submitted = true;
    emit sig_submittedChanged();
    emit sig_cancel();
}
