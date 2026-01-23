#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Nils Schimmelmann <nschimme@gmail.com> (Jahara)

#include "../global/AnsiTextUtils.h"
#include "../global/macros.h"
#include "remoteeditsession.h"

#include <memory>

#include <QAction>
#include <QDialog>
#include <QPlainTextEdit>
#include <QScopedPointer>
#include <QSize>
#include <QString>
#include <QtCore>

class AnsiViewWindow;
class QCloseEvent;
class QMenu;
class QMenuBar;
class QObject;
class QPlainTextEdit;
class QStackedWidget;
class QStatusBar;
class QTextEdit;
class QVBoxLayout;
class QWidget;
class GotoWidget;
class FindReplaceWidget;

enum class NODISCARD EditViewCmdEnum { VIEW_OPTION, EDIT_ALIGNMENT, EDIT_COLORS, EDIT_WHITESPACE };
enum class NODISCARD EditCmd2Enum { EDIT_ONLY, EDIT_OR_VIEW, SPACER };

struct NODISCARD EditViewCommand final
{
    using mem_fn_ptr_type = void (RemoteEditWidget::*)();
    const mem_fn_ptr_type mem_fn_ptr;
    const EditViewCmdEnum cmd_type;
    const char *const action;
    const char *const status;
    const char *const shortcut;

    constexpr explicit EditViewCommand(const mem_fn_ptr_type _mem_fn_ptr,
                                       const EditViewCmdEnum _cmd_type,
                                       const char *const _action,
                                       const char *const _status,
                                       const char *const _shortcut)
        : mem_fn_ptr{_mem_fn_ptr}
        , cmd_type{_cmd_type}
        , action{_action}
        , status{_status}
        , shortcut{_shortcut}
    {}
};

struct NODISCARD EditCommand2 final
{
    enum class StdAction { None, Undo, Redo, SelectAll, Cut, Copy, Paste };
    StdAction stdAction = StdAction::None;
    const char *theme = nullptr;
    const char *icon = nullptr;
    const char *action = nullptr;
    const char *status = nullptr;
    const char *shortcut = nullptr;
    EditCmd2Enum cmd_type = EditCmd2Enum::SPACER;

    constexpr EditCommand2() = default;
    constexpr explicit EditCommand2(const StdAction _stdAction,
                                    const char *const _theme,
                                    const char *const _icon,
                                    const char *const _action,
                                    const char *const _status,
                                    const char *const _shortcut,
                                    const EditCmd2Enum _cmd_type)
        : stdAction{_stdAction}
        , theme{_theme}
        , icon{_icon}
        , action{_action}
        , status{_status}
        , shortcut{_shortcut}
        , cmd_type{_cmd_type}
    {}
};

// NOTE: Ctrl+A is "Select All" by default.
#define XFOREACH_REMOTE_EDIT_MENU_ITEM(X) \
    X(justifyText, \
      EditViewCmdEnum::EDIT_ALIGNMENT, \
      "&Justify Entire Message", \
      "Justify text to 80 characters", \
      nullptr) \
    X(justifyLines, \
      EditViewCmdEnum::EDIT_ALIGNMENT, \
      "Justify &Selection", \
      "Justify selection to 80 characters", \
      "Ctrl+J") \
    X(expandTabs, \
      EditViewCmdEnum::EDIT_WHITESPACE, \
      "&Expand Tabs", \
      "Expand tabs to 8-character tabstops", \
      "Ctrl+E") \
    X(removeTrailingWhitespace, \
      EditViewCmdEnum::EDIT_WHITESPACE, \
      "Remove Trailing &Whitespace", \
      "Remove trailing whitespace", \
      "Ctrl+W") \
    X(removeDuplicateSpaces, \
      EditViewCmdEnum::EDIT_WHITESPACE, \
      "Remove &Duplicate Spaces", \
      "Remove duplicate spaces in any partly-selected lines", \
      "Ctrl+D") \
    X(normalizeAnsi, \
      EditViewCmdEnum::EDIT_COLORS, \
      "&Normalize Ansi Codes", \
      "Normalize ansi codes.", \
      "Ctrl+N") \
    X(insertAnsiReset, \
      EditViewCmdEnum::EDIT_COLORS, \
      "&Insert Ansi Reset Code", \
      "Insert an ansi reset code (ESC[0m).", \
      "Ctrl+I") \
    X(joinLines, \
      EditViewCmdEnum::EDIT_ALIGNMENT, \
      "Joi&n Lines", \
      "Join all partly-selected lines.", \
      "Ctrl+Shift+J") \
    X(quoteLines, \
      EditViewCmdEnum::EDIT_ALIGNMENT, \
      "&Quote Lines", \
      "Add a quote prefix to all partly-selected lines.", \
      "Ctrl+>" /* aka "Ctrl+Shift+." */) \
    X(previewAnsi, \
      EditViewCmdEnum::VIEW_OPTION, \
      "&Preview Ansi Codes", \
      "Preview message with ansi coloring.", \
      "Ctrl+P") \
    X(toggleWhitespace, \
      EditViewCmdEnum::VIEW_OPTION, \
      "Toggle &Whitespace", \
      "Toggle the display of whitespace.", \
      "Ctrl+Shift+W")

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

private:
    void handleEventTab(QKeyEvent *event);
    void handleEventBacktab(QKeyEvent *event);
    void handle_toolTip(QEvent *event) const;
};

class NODISCARD_QOBJECT RemoteEditWidget : public QDialog
{
    Q_OBJECT

private:
    QMenuBar *m_menuBar;
    QStatusBar *m_statusBar;

public:
    using Editor = RemoteTextEdit;

private:
    const bool m_editSession;
    const QString m_title;
    const QString m_body;

    bool m_submitted = false;

    QStackedWidget *m_stackedWidget = nullptr;
    Editor *m_sourceEdit = nullptr;
    QTextEdit *m_wysiwygEdit = nullptr;
    bool m_wysiwygMode = false;
    AnsiSupportFlags m_exportFlags = ANSI_COLOR_SUPPORT_HI;
    QMenu *m_formatMenu = nullptr;

    QScopedPointer<GotoWidget> m_gotoWidget;
    QScopedPointer<FindReplaceWidget> m_findReplaceWidget;
    std::unique_ptr<AnsiViewWindow> m_preview;

public:
    explicit RemoteEditWidget(bool editSession, QString title, QString body, QWidget *parent);
    ~RemoteEditWidget() override;

public:
    NODISCARD QSize minimumSizeHint() const override;
    NODISCARD QSize sizeHint() const override;
    void closeEvent(QCloseEvent *event) override;

protected:
    void showEvent(QShowEvent *event) override;

private:
    NODISCARD Editor *createTextEdit();
    NODISCARD QTextEdit *createWysiwygEdit();
    NODISCARD GotoWidget *createGotoWidget();
    NODISCARD FindReplaceWidget *createFindReplaceWidget();

    void addToMenu(QMenu *menu, const EditViewCommand &cmd);
    void addToMenu(QMenu *menu, const EditCommand2 &cmd, const QObject *pReceiver);

    void addFileMenu();
    void addEditAndViewMenus();
    void addFormatMenu();
    void addSave(QMenu *fileMenu);
    void addExit(QMenu *fileMenu);
    void addExportModeMenu(QMenu *fileMenu);
    void addStatusBar();

    void syncToWysiwyg();
    void syncToSource();
    NODISCARD QString getCurrentText() const;

signals:
    void sig_cancel();
    void sig_save(const QString &);

protected slots:
    void slot_cancelEdit();
    void slot_finishEdit();
    NODISCARD bool slot_maybeCancel();
    NODISCARD bool slot_contentsChanged() const;
    void slot_updateStatusBar();
    void slot_updateStatus(const QString &message);
    void slot_handleFindRequested(const QString &term, QTextDocument::FindFlags flags);
    void slot_handleReplaceCurrentRequested(const QString &findTerm,
                                            const QString &replaceTerm,
                                            QTextDocument::FindFlags flags);
    void slot_handleReplaceAllRequested(const QString &findTerm,
                                        const QString &replaceTerm,
                                        QTextDocument::FindFlags flags);

    void slot_toggleWysiwygMode();
    void slot_setExportMode16();
    void slot_setExportMode256();
    void slot_setExportModeRGB();
    void slot_setFgColor();
    void slot_setBgColor();
    void slot_clearFormatting();
    void slot_setBold();
    void slot_setItalic();
    void slot_setUnderline();

#define X_DECLARE_SLOT(a, b, c, d, e) void slot_##a();
    XFOREACH_REMOTE_EDIT_MENU_ITEM(X_DECLARE_SLOT)
#undef X_DECLARE_SLOT
};
