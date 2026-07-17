#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "../global/macros.h"

#include <memory>

#include <QObject>
#include <QString>
#include <QVariantMap>

class QQuickTextDocument;
class QTextDocument;
class RemoteEditHighlighter;
class QDialog;

// Widget-free controller for the QML port of RemoteEditWidget/RemoteTextEdit
// (see mpi/remoteeditwidget.h). Owns no widgets: it holds a raw (non-owning)
// pointer to the QTextDocument backing a QML TextArea (handed in via
// attachDocument()) and drives every text operation through
// RemoteEditDocumentOps.h's free functions, exactly like the widget's slots
// do via m_textEdit->textCursor(). This step only adds the controller/dialog;
// nothing in the session layer (remoteeditsession.cpp/remoteedit.cpp)
// constructs this yet, so it has no effect on the still-active
// RemoteEditWidget.
//
// QML contract (stub-drift guard: TestQml.cpp exercises this surface
// directly against a real controller rather than a stub, since this class
// has no widget/MapData dependency to route around):
//   Q_PROPERTY QString title / QString body / bool isEditSession -- CONSTANT,
//     set once from the constructor args. "body" is the *initial* text only;
//     QML should assign it to the TextArea's "text" exactly once
//     (Component.onCompleted), matching the widget's constructor -- text
//     edits after that flow through the shared QTextDocument, never through
//     re-reading this property.
//   Q_PROPERTY QString fontFamily / int fontPointSize -- CONSTANT, resolved
//     once from getConfig().integratedClient.font, matching
//     RemoteEditWidget::createTextEdit()'s QFont::fromString() call.
//   Q_PROPERTY bool dirty -- NOTIFY sig_dirtyChanged; recomputed on every
//     QTextDocument::contentsChanged from the attached document by
//     byte-comparing toPlainText() against the original "body" (mirrors
//     slot_contentsChanged() -- NOT QTextDocument::isModified()).
//   Q_PROPERTY bool submitted -- NOTIFY sig_submittedChanged; true once
//     submit() or cancel() has been called (mirrors m_submitted).
//   Q_PROPERTY bool showingWhitespace -- NOTIFY sig_showingWhitespaceChanged;
//     mirrors RemoteTextEdit::isShowingWhitespace().
//   Q_PROPERTY QString statusText -- NOTIFY sig_statusChanged; the formatted
//     status-bar line, in the exact format
//     RemoteEditWidget::slot_updateStatusBar() produces. QML must call
//     updateStatus() below whenever the TextArea's cursorPosition/
//     selectionStart/selectionEnd/text change to keep this current (the
//     controller cannot observe the QML TextArea's cursor on its own).
//   Q_INVOKABLE attachDocument(QQuickTextDocument*) -- must be called once,
//     from Component.onCompleted after the TextArea exists (its
//     "textDocument" attached/plain property, per Qt 6.4's
//     QQuickTextDocument). Installs a RemoteEditHighlighter on the document,
//     sets AddSpaceForLineAndParagraphSeparators + the initial
//     ShowTabsAndSpaces state, and connects contentsChanged to recompute
//     dirty. Every other invokable below is a no-op (or returns a
//     not-found/empty result) until this has been called.
//   Q_INVOKABLE updateStatus(int selectionStart, int selectionEnd, int
//     cursorPosition) -- rebuilds a QTextCursor from the three ints exactly
//     as QQC2 TextArea reports them (selectionStart/selectionEnd are always
//     the min/max, cursorPosition is the actual caret; the anchor is
//     whichever of selectionStart/selectionEnd isn't the caret), then
//     recomputes statusText via computeRemoteEditStatus(). Call from
//     onCursorPositionChanged/onSelectionChanged/onTextChanged.
//   Q_INVOKABLE justifyAll() -- whole-document justify (File has no
//     selection concept); replaces the entire document text like
//     RemoteTextEdit::replaceAll() (selectAll + insertPlainText, preserving
//     undo).
//   Q_INVOKABLE justifySelection/expandTabs/removeTrailingWhitespace/
//     removeDuplicateSpaces/joinLines/quoteLines(int selectionStart, int
//     selectionEnd) -- each builds a forward QTextCursor
//     (anchor=selectionStart, position=selectionEnd) and calls the matching
//     RemoteEditDocumentOps.h free function, which wraps its own undo group.
//   Q_INVOKABLE normalizeAnsi() -- whole-document, like justifyAll(); a no-op
//     when the text has no ANSI codes (matches remoteEditNormalizeAnsi()'s
//     std::nullopt case).
//   Q_INVOKABLE int insertAnsiReset(int cursorPosition) -- inserts an ANSI
//     reset code at cursorPosition; returns the new caret position (QML must
//     assign it back to TextArea.cursorPosition, since the controller can't
//     reach into the TextArea itself).
//   Q_INVOKABLE void indentSelection(int selectionStart, int selectionEnd) --
//     Tab-with-selection: 2-space block indent of every partly-selected line
//     (mirrors RemoteTextEdit::handleEventTab()'s selection branch).
//   Q_INVOKABLE int insertTabExpanded(int cursorPosition) -- Tab-without-
//     selection: inserts a real tab, expands tabs on that line, and returns
//     the caret position after the expansion (mirrors handleEventTab()'s
//     other branch); QML must assign it back to TextArea.cursorPosition.
//   Q_INVOKABLE void unindentSelection(int selectionStart, int selectionEnd)
//     -- Backtab, with or without a selection (mirrors handleEventBacktab()).
//   Q_INVOKABLE void toggleWhitespace() -- flips showingWhitespace and the
//     document's QTextOption::ShowTabsAndSpaces flag.
//   Q_INVOKABLE void previewAnsi() -- opens makeAnsiViewWindow() on the
//     current document text (trailing newline ensured), held in a
//     unique_ptr<QDialog> member exactly like RemoteEditWidget::m_preview
//     (see AnsiViewWindow.h's doc comment for why it's QDialog, not
//     AnsiViewWindow).
//   Q_INVOKABLE QVariantMap find(QString term, bool backwards, int
//     selectionStart, int selectionEnd) -- {found: bool, start: int, end:
//     int, message: QString}; start/end are only meaningful when found is
//     true (QML should call TextArea.select(start, end) then). Wraps around
//     the document like remoteEditFind(); message matches
//     slot_handleFindRequested()'s three cases verbatim.
//   Q_INVOKABLE QVariantMap replaceCurrent(QString findTerm, QString
//     replaceTerm, int selectionStart, int selectionEnd) -- ports
//     slot_handleReplaceCurrentRequested() exactly, including its quirk that
//     the "Replaced: ..." status message is always immediately overwritten
//     by the subsequent find-next call, so the returned "message" is always
//     the find-next outcome; "replaced" reports whether the replace itself
//     happened. {replaced: bool, found: bool, start: int, end: int, message:
//     QString}.
//   Q_INVOKABLE QVariantMap replaceAll(QString findTerm, QString
//     replaceTerm) -- {count: int, message: QString}; ports
//     slot_handleReplaceAllRequested(). Does not touch the caret -- like the
//     widget, QML is expected to leave its own cursor position/selection
//     numerically unchanged afterward (a deliberately naive restore; text
//     around that offset may have shifted).
//   Q_INVOKABLE QVariantMap gotoLine(int lineNum) -- {found: bool, start:
//     int, message: QString}; ports the GotoWidget handler in
//     RemoteEditWidget::createGotoWidget()'s lambda.
//   Q_INVOKABLE void submit() -- edit-session only; sets submitted, emits
//     sig_save(current document text), mirrors slot_finishEdit(). A no-op
//     (asserts in debug) when !isEditSession.
//   Q_INVOKABLE void cancel() -- sets submitted, emits sig_cancel(), mirrors
//     slot_cancelEdit().
//   Q_INVOKABLE bool isDocumentDirty() const -- convenience wrapper so C++
//     hosts (RemoteEditDialogHost's closeEvent) can read dirty without going
//     through the Q_PROPERTY meta-call; identical to the "dirty" property.
// Signals: sig_save(QString), sig_cancel(), sig_dirtyChanged(),
// sig_submittedChanged(), sig_statusChanged(), sig_showingWhitespaceChanged().
class NODISCARD_QOBJECT RemoteEditController final : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString title READ getTitle CONSTANT)
    Q_PROPERTY(QString body READ getBody CONSTANT)
    Q_PROPERTY(bool isEditSession READ getIsEditSession CONSTANT)
    Q_PROPERTY(QString fontFamily READ getFontFamily CONSTANT)
    Q_PROPERTY(int fontPointSize READ getFontPointSize CONSTANT)
    Q_PROPERTY(bool dirty READ getDirty NOTIFY sig_dirtyChanged)
    Q_PROPERTY(bool submitted READ getSubmitted NOTIFY sig_submittedChanged)
    Q_PROPERTY(bool showingWhitespace READ getShowingWhitespace NOTIFY sig_showingWhitespaceChanged)
    Q_PROPERTY(QString statusText READ getStatusText NOTIFY sig_statusChanged)

private:
    const bool m_editSession;
    const QString m_title;
    const QString m_body;
    QString m_fontFamily;
    int m_fontPointSize = -1;

    QTextDocument *m_document = nullptr;            // non-owning; owned by the QQuickTextDocument
    RemoteEditHighlighter *m_highlighter = nullptr; // owned by m_document (QObject parent)
    std::unique_ptr<QDialog> m_preview;

    bool m_dirty = false;
    bool m_submitted = false;
    bool m_showingWhitespace = false;
    QString m_statusText = QStringLiteral("Ready");

public:
    explicit RemoteEditController(bool editSession,
                                  QString title,
                                  QString body,
                                  QObject *parent = nullptr);
    ~RemoteEditController() final;

public:
    NODISCARD QString getTitle() const { return m_title; }
    NODISCARD QString getBody() const { return m_body; }
    NODISCARD bool getIsEditSession() const { return m_editSession; }
    NODISCARD QString getFontFamily() const { return m_fontFamily; }
    NODISCARD int getFontPointSize() const { return m_fontPointSize; }
    NODISCARD bool getDirty() const { return m_dirty; }
    NODISCARD bool getSubmitted() const { return m_submitted; }
    NODISCARD bool getShowingWhitespace() const { return m_showingWhitespace; }
    NODISCARD QString getStatusText() const { return m_statusText; }

    NODISCARD Q_INVOKABLE bool isDocumentDirty() const { return m_dirty; }

    Q_INVOKABLE void attachDocument(QQuickTextDocument *doc);

    Q_INVOKABLE void updateStatus(int selectionStart, int selectionEnd, int cursorPosition);

    Q_INVOKABLE void justifyAll();
    Q_INVOKABLE void justifySelection(int selectionStart, int selectionEnd);
    Q_INVOKABLE void expandTabs(int selectionStart, int selectionEnd);
    Q_INVOKABLE void removeTrailingWhitespace(int selectionStart, int selectionEnd);
    Q_INVOKABLE void removeDuplicateSpaces(int selectionStart, int selectionEnd);
    Q_INVOKABLE void normalizeAnsi();
    NODISCARD Q_INVOKABLE int insertAnsiReset(int cursorPosition);
    Q_INVOKABLE void joinLines(int selectionStart, int selectionEnd);
    Q_INVOKABLE void quoteLines(int selectionStart, int selectionEnd);

    Q_INVOKABLE void indentSelection(int selectionStart, int selectionEnd);
    NODISCARD Q_INVOKABLE int insertTabExpanded(int cursorPosition);
    Q_INVOKABLE void unindentSelection(int selectionStart, int selectionEnd);

    Q_INVOKABLE void toggleWhitespace();
    Q_INVOKABLE void previewAnsi();

    NODISCARD Q_INVOKABLE QVariantMap find(const QString &term,
                                           bool backwards,
                                           int selectionStart,
                                           int selectionEnd);
    NODISCARD Q_INVOKABLE QVariantMap replaceCurrent(const QString &findTerm,
                                                     const QString &replaceTerm,
                                                     int selectionStart,
                                                     int selectionEnd);
    NODISCARD Q_INVOKABLE QVariantMap replaceAll(const QString &findTerm,
                                                 const QString &replaceTerm);
    NODISCARD Q_INVOKABLE QVariantMap gotoLine(int lineNum);

    Q_INVOKABLE void submit();
    Q_INVOKABLE void cancel();

private:
    void recomputeDirty();

signals:
    void sig_save(const QString &text);
    void sig_cancel();

    void sig_dirtyChanged();
    void sig_submittedChanged();
    void sig_showingWhitespaceChanged();
    void sig_statusChanged();
};
