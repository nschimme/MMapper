#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Nils Schimmelmann <nschimme@gmail.com> (Jahara)

#include "../client/PaletteManager.h"
#include "../global/RuleOf5.h"
#include "../global/macros.h"

#include <list>
#include <optional>
#include <string>

#include <QPlainTextEdit>
#include <QPoint>

class QEvent;
class QKeyEvent;
class QObject;
class QWidget;

// Key classification system for unified key handling
enum class KeyType {
    FunctionKey,      // F1-F12
    NumpadKey,        // NUMPAD0-9, NUMPAD_SLASH, etc.
    NavigationKey,    // HOME, END, INSERT
    ArrowKey,         // UP, DOWN (for history), LEFT, RIGHT (for hotkeys)
    MiscKey,          // ACCENT, number row, HYPHEN, EQUAL
    TerminalShortcut, // Ctrl+U, Ctrl+W, Ctrl+H
    BasicKey,         // Enter, Tab (no modifiers)
    PageKey,          // PageUp, PageDown (for scrolling display)
    Other             // Not handled by us
};

struct NODISCARD KeyClassification
{
    KeyType type = KeyType::Other;
    QString keyName;
    Qt::KeyboardModifiers realModifiers = Qt::NoModifier;
    bool shouldHandle = false;
};

class NODISCARD InputHistory final : private std::list<QString>
{
private:
    using base = std::list<QString>;
    base::const_iterator m_iterator = begin();

public:
    using base::empty;
    void addInputLine(const QString &);

    void forward() { ++m_iterator; }
    void backward() { --m_iterator; }
    void reset() { m_iterator = begin(); }

    NODISCARD bool atEnd() const { return m_iterator == end(); }
    NODISCARD bool atFront() const { return m_iterator == begin(); }
    NODISCARD const QString &value() const { return *m_iterator; }
};

class NODISCARD TabHistory final : private std::list<QString>
{
private:
    using base = std::list<QString>;
    base::const_iterator m_iterator = begin();

public:
    using base::empty;
    void addInputLine(const QString &);

    void forward() { ++m_iterator; }
    void reset() { m_iterator = begin(); }

    NODISCARD bool atEnd() const { return m_iterator == end(); }
    NODISCARD const QString &value() const { return *m_iterator; }
};

struct NODISCARD InputWidgetOutputs
{
    virtual ~InputWidgetOutputs();
    void sendUserInput(const QString &msg) { virt_sendUserInput(msg); }
    void displayMessage(const QString &msg) { virt_displayMessage(msg); }
    void showMessage(const QString &msg, const int timeout) { virt_showMessage(msg, timeout); }
    void gotPasswordInput(const QString &password) { virt_gotPasswordInput(password); }
    void scrollDisplay(bool pageUp) { virt_scrollDisplay(pageUp); }

private:
    virtual void virt_sendUserInput(const QString &msg) = 0;
    virtual void virt_displayMessage(const QString &msg) = 0;
    virtual void virt_showMessage(const QString &msg, int timeout) = 0;
    virtual void virt_gotPasswordInput(const QString &password) = 0;
    virtual void virt_scrollDisplay(bool pageUp) = 0;
};

class NODISCARD_QOBJECT InputWidget final : public QPlainTextEdit
{
    Q_OBJECT
private:
    using base = QPlainTextEdit;
    InputWidgetOutputs &m_outputs;
    TabHistory m_tabHistory;
    QString m_tabFragment;
    InputHistory m_inputHistory;
    PaletteManager m_paletteManager;
    bool m_tabbing = false;
    bool m_handledInShortcutOverride = false; // Track if key was already handled in ShortcutOverride

public:
    explicit InputWidget(QWidget *parent, InputWidgetOutputs &);
    ~InputWidget() final;

    DELETE_CTORS_AND_ASSIGN_OPS(InputWidget);
    NODISCARD QSize sizeHint() const final;

protected:
    void keyPressEvent(QKeyEvent *) final;
    NODISCARD bool event(QEvent *) final;

private:
    void gotInput();
    NODISCARD bool tryHistory(int);
    NODISCARD bool numpadKeyPressed(int key, Qt::KeyboardModifiers modifiers);
    NODISCARD bool navigationKeyPressed(int key, Qt::KeyboardModifiers modifiers);
    NODISCARD bool arrowKeyPressed(int key, Qt::KeyboardModifiers modifiers);
    NODISCARD bool miscKeyPressed(int key, Qt::KeyboardModifiers modifiers);
    void functionKeyPressed(int key, Qt::KeyboardModifiers modifiers);
    NODISCARD QString buildHotkeyString(const QString &keyName, Qt::KeyboardModifiers modifiers);
    NODISCARD bool handleTerminalShortcut(int key);
    NODISCARD bool handleBasicKey(int key);
    NODISCARD bool handlePageKey(int key, Qt::KeyboardModifiers modifiers);

private:
    void tabComplete();
    void backwardHistory();
    void forwardHistory();

private:
    void sendUserInput(const QString &msg) { m_outputs.sendUserInput(msg); }
    void sendCommandWithSeparator(const QString &command);
};
