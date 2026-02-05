// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "inputwidget.h"
#include <QKeyEvent>

InputWidget::InputWidget(InputViewModel *vm, QWidget *parent)
    : QPlainTextEdit(parent), m_viewModel(vm)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setFont(m_viewModel->font());
    setLineWrapMode(QPlainTextEdit::NoWrap);
    m_paletteManager.init(*this, std::nullopt, Qt::lightGray);

    connect(m_viewModel, &InputViewModel::currentTextChanged, this, [this]() {
        if (toPlainText() != m_viewModel->currentText()) setPlainText(m_viewModel->currentText());
    });
    connect(m_viewModel, &InputViewModel::sig_tabCompletionAvailable, this, [this](const QString &c, int f) {
        QTextCursor cur = textCursor(); cur.movePosition(QTextCursor::Left, QTextCursor::KeepAnchor, cur.selectedText().length());
        cur.insertText(c); cur.movePosition(QTextCursor::Left, QTextCursor::MoveAnchor, c.length() - f);
        cur.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, c.length() - f);
        setTextCursor(cur);
    });
}

InputWidget::~InputWidget() = default;
QSize InputWidget::sizeHint() const { return minimumSize(); }

void InputWidget::keyPressEvent(QKeyEvent *e) {
    if (m_handledInShortcutOverride) { m_handledInShortcutOverride = false; e->accept(); return; }
    auto k = static_cast<Qt::Key>(e->key()); auto m = e->modifiers();
    if (m_tabbing && (k == Qt::Key_Backspace || k == Qt::Key_Escape)) { textCursor().removeSelectedText(); m_tabbing = false; e->accept(); return; }
    if (k != Qt::Key_Tab) m_tabbing = false;
    if (handleCommandInput(k, m)) { e->accept(); return; }
    if (k == Qt::Key_Up) { m_viewModel->prevHistory(); e->accept(); }
    else if (k == Qt::Key_Down) { m_viewModel->nextHistory(); e->accept(); }
    else if (k == Qt::Key_Tab) { tabComplete(); e->accept(); }
    else if (k == Qt::Key_Return || k == Qt::Key_Enter) { handleInput(); e->accept(); }
    else if (k == Qt::Key_PageUp || k == Qt::Key_PageDown) { emit m_viewModel->sig_scrollDisplay(k == Qt::Key_PageUp); e->accept(); }
    else QPlainTextEdit::keyPressEvent(e);
}

bool InputWidget::handleCommandInput(Qt::Key k, Qt::KeyboardModifiers m) {
    if (m & Qt::ControlModifier && handleTerminalShortcut(static_cast<int>(k))) return true;
    return false;
}

bool InputWidget::handleTerminalShortcut(int k) {
    if (k == Qt::Key_H) { textCursor().deletePreviousChar(); return true; }
    if (k == Qt::Key_U) { clear(); return true; }
    if (k == Qt::Key_W) {
        QTextCursor c = textCursor(); if (c.atStart()) return true;
        while (!c.atStart() && document()->characterAt(c.position()-1).isSpace()) c.movePosition(QTextCursor::PreviousCharacter, QTextCursor::KeepAnchor);
        while (!c.atStart() && !document()->characterAt(c.position()-1).isSpace()) c.movePosition(QTextCursor::PreviousCharacter, QTextCursor::KeepAnchor);
        c.removeSelectedText(); setTextCursor(c); return true;
    }
    return false;
}

void InputWidget::handleInput() { m_viewModel->submitInput(toPlainText()); clear(); }

void InputWidget::tabComplete() {
    if (!m_tabbing) {
        QTextCursor cur = textCursor();
        while (cur.position() > 0 && !document()->characterAt(cur.position()-1).isSpace()) cur.movePosition(QTextCursor::PreviousCharacter, QTextCursor::KeepAnchor);
        m_tabFragment = cur.selectedText(); if (m_tabFragment.isEmpty()) return;
        m_tabbing = true; m_viewModel->tabComplete(m_tabFragment, true);
    } else m_viewModel->tabComplete(m_tabFragment, false);
}

bool InputWidget::event(QEvent *e) {
    if (e->type() == QEvent::ShortcutOverride) {
        auto *ke = static_cast<QKeyEvent*>(e);
        if (handleCommandInput(static_cast<Qt::Key>(ke->key()), ke->modifiers())) { m_handledInShortcutOverride = true; e->accept(); return true; }
    }
    m_paletteManager.tryUpdateFromFocusEvent(*this, e->type());
    return QPlainTextEdit::event(e);
}
