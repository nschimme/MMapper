#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Nils Schimmelmann <nschimme@gmail.com> (Jahara)

#include "../global/macros.h"
#include "remoteeditsession.h"

#include <memory>

#include <QAction>
#include <QMainWindow>
#include <QPlainTextEdit>
#include <QScopedPointer>
#include <QSize>
#include <QString>
#include <QtCore>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QVBoxLayout>

struct EditViewCommand;
struct EditCommand2;

class AnsiViewWindow;
class FindReplaceDialog; // Added
class QCloseEvent;
class QMenu;
class QMenuBar;
class QObject;
class QPlainTextEdit;
class QWidget;
class QStatusBar;

enum class NODISCARD EditViewCmdEnum {
    VIEW_OPTION, /* For view-related options like preview, toggle whitespace */
    EDIT_ALIGNMENT, /* For text alignment and justification commands */
    EDIT_COLORS, /* For ANSI color related commands */
    EDIT_WHITESPACE, /* For commands related to whitespace manipulation */
    EDIT_FIND_REPLACE /* For Find/Replace and Go to Line actions */
};
enum class NODISCARD EditCmd2Enum { EDIT_ONLY, EDIT_OR_VIEW, SPACER };

// NOTE: Ctrl+A is "Select All" by default.
// XMACRO to define menu items for the RemoteEditWidget.
// Arguments: internal_name, EditViewCmdEnum category, UI display text, status tip text, shortcut string (or nullptr)
#define XFOREACH_REMOTE_EDIT_MENU_ITEM(X) \
    X(justifyText, \
      EditViewCmdEnum::EDIT_ALIGNMENT, \
      "&Justify Entire Message", \
      "Justify text to 80 characters", \
      QVariant()) /* nullptr -> QVariant() for invalid/no shortcut */ \
    X(justifyLines, \
      EditViewCmdEnum::EDIT_ALIGNMENT, \
      "Justify &Selection", \
      "Justify selection to 80 characters", \
      QVariant("Ctrl+J")) \
    X(expandTabs, \
      EditViewCmdEnum::EDIT_WHITESPACE, \
      "&Expand Tabs", \
      "Expand tabs to 8-character tabstops", \
      QVariant("Ctrl+E")) \
    X(removeTrailingWhitespace, \
      EditViewCmdEnum::EDIT_WHITESPACE, \
      "Remove Trailing &Whitespace", \
      "Remove trailing whitespace", \
      QVariant("Ctrl+W")) \
    X(removeDuplicateSpaces, \
      EditViewCmdEnum::EDIT_WHITESPACE, \
      "Remove &Duplicate Spaces", \
      "Remove duplicate spaces in any partly-selected lines", \
      QVariant("Ctrl+D")) \
    X(normalizeAnsi, \
      EditViewCmdEnum::EDIT_COLORS, \
      "&Normalize Ansi Codes", \
      "Normalize ansi codes.", \
      QVariant("Ctrl+N")) \
    X(insertAnsiReset, \
      EditViewCmdEnum::EDIT_COLORS, \
      "&Insert Ansi Reset Code", \
      "Insert an ansi reset code (ESC[0m).", \
      QVariant("Ctrl+I")) \
    X(joinLines, \
      EditViewCmdEnum::EDIT_ALIGNMENT, \
      "Joi&n Lines", \
      "Join all partly-selected lines.", \
      QVariant("Ctrl+Shift+J")) \
    X(quoteLines, \
      EditViewCmdEnum::EDIT_ALIGNMENT, \
      "&Quote Lines", \
      "Add a quote prefix to all partly-selected lines.", \
      QVariant("Ctrl+>")) /* aka "Ctrl+Shift+." */ \
    X(previewAnsi, \
      EditViewCmdEnum::VIEW_OPTION, \
      "&Preview Ansi Codes", \
      "Preview message with ansi coloring.", \
      QVariant("Ctrl+P")) \
    X(toggleWhitespace, \
      EditViewCmdEnum::VIEW_OPTION, \
      "Toggle &Whitespace", \
      "Toggle the display of whitespace.", \
      QVariant("Ctrl+Shift+W")) \
    /* Find/Replace Actions */ \
    X(findReplace, \
      EditViewCmdEnum::EDIT_FIND_REPLACE, \
      "&Find/Replace...", \
      "Show the find/replace dialog.", \
      QVariant(QKeySequence::Find)) \
    X(findNext, \
      EditViewCmdEnum::EDIT_FIND_REPLACE, \
      "Find &Next", \
      "Find next occurrence of the search text.", \
      QVariant(QKeySequence::FindNext)) \
    X(findPrevious, \
      EditViewCmdEnum::EDIT_FIND_REPLACE, \
      "Find &Previous", \
      "Find previous occurrence of the search text.", \
      QVariant(QKeySequence::FindPrevious)) \
    X(replace, \
      EditViewCmdEnum::EDIT_FIND_REPLACE, \
      "&Replace", \
      "Replace the current selection with the replacement text and find next.", \
      QVariant(QKeySequence::Replace)) \
    /* Go to Line Action */ \
    X(gotoLine, \
      EditViewCmdEnum::EDIT_FIND_REPLACE, \
      "&Go to Line...", \
      "Go to a specific line number in the document.", \
      QVariant("Ctrl+G"))

class NODISCARD_QOBJECT RemoteTextEdit final : public QPlainTextEdit
{
    Q_OBJECT

private:
    using base = QPlainTextEdit;

public:
    explicit RemoteTextEdit(const QString &initialText, QWidget *parent);
    ~RemoteTextEdit() final;

protected:
    void keyPressEvent(QKeyEvent *event) override;
    bool event(QEvent *event) override;

private:
    /// Purposely hides base::setPlainText(), because it clears the UNDO history.
    /// You should call replaceAll() instead.
    void setPlainText(const QString &str) = delete;

public:
    void replaceAll(const QString &str);
    void showWhitespace(bool enabled);
    NODISCARD bool isShowingWhitespace() const;
    void toggleWhitespace();

    void joinLines();
    void quoteLines();
    void justifyLines(int maxLength);

public:
    void prefixPartialSelection(const QString &prefix);

    // Find/Replace and Goto Line methods
    /// Finds the next occurrence of the given text.
    /// @param text The text to find.
    /// @param flags The search flags (e.g., case sensitivity).
    /// @return True if the text was found, false otherwise.
    bool findNext(const QString &text, QTextDocument::FindFlags flags);

    /// Finds the previous occurrence of the given text.
    /// @param text The text to find.
    /// @param flags The search flags (e.g., case sensitivity).
    /// @return True if the text was found, false otherwise.
    bool findPrevious(const QString &text, QTextDocument::FindFlags flags);

    /// Replaces the current selection if it matches findText and then finds the next occurrence.
    /// @param findText The text to find.
    /// @param replaceText The text to replace with.
    /// @param flags The search flags.
    void replace(const QString &findText, const QString &replaceText, QTextDocument::FindFlags flags);

    /// Replaces all occurrences of findText with replaceText.
    /// @param findText The text to find.
    /// @param replaceText The text to replace with.
    /// @param flags The search flags.
    /// @return The number of replacements made.
    int replaceAll(const QString &findText, const QString &replaceText, QTextDocument::FindFlags flags);

    /// Moves the cursor to the beginning of the specified line number.
    /// @param lineNum The 0-indexed line number to go to.
    void gotoLine(int lineNum);


private:
    void handleEventTab(QKeyEvent *event);
    void handleEventBacktab(QKeyEvent *event);
    void handle_toolTip(QEvent *event) const;
};

class NODISCARD_QOBJECT RemoteEditWidget : public QMainWindow
{
    Q_OBJECT

public:
    using Editor = RemoteTextEdit;

private:
    const bool m_editSession;
    const QString m_title;
    const QString m_body;

    bool m_submitted = false;
    QScopedPointer<Editor> m_textEdit; // The main text editing widget
    std::unique_ptr<AnsiViewWindow> m_preview; // Window for ANSI preview
    FindReplaceDialog *m_findReplaceDialog = nullptr; // Modeless find/replace dialog instance

public:
    explicit RemoteEditWidget(bool editSession, QString title, QString body, QWidget *parent);
    ~RemoteEditWidget() override;

public:
    NODISCARD QSize minimumSizeHint() const override;
    NODISCARD QSize sizeHint() const override;
    void closeEvent(QCloseEvent *event) override;

public:
    void setVisible(bool visible) override;

private:
    NODISCARD Editor *createTextEdit();

    void addToMenu(QMenu *menu, const EditViewCommand &cmd);
    void addToMenu(QMenu *menu, const EditCommand2 &cmd, const Editor *pTextEdit);

    void addFileMenu(QMenuBar *menuBar, const Editor *pTextEdit);
    void addEditAndViewMenus(QMenuBar *menuBar, const Editor *pTextEdit);
    void addSave(QMenu *fileMenu);
    void addExit(QMenu *fileMenu);
    void addStatusBar(const Editor *pTextEdit);

signals:
    void sig_cancel();
    void sig_save(const QString &);

protected slots:
    void slot_cancelEdit();
    void slot_finishEdit();
    NODISCARD bool slot_maybeCancel();
    NODISCARD bool slot_contentsChanged() const;
    void slot_updateStatusBar();
    void slot_replaceAll(); // Added: Slot for handling replaceAll signal from FindReplaceDialog

#define X_DECLARE_SLOT(a, b, c, d, e) void slot_##a();
    XFOREACH_REMOTE_EDIT_MENU_ITEM(X_DECLARE_SLOT)
#undef X_DECLARE_SLOT
};
