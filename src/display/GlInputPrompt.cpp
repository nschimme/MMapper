// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "GlInputPrompt.h"

#include <algorithm>

#include <QClipboard>
#include <QGuiApplication>

namespace mmgl {

void GlInputPrompt::setText(const QString &text)
{
    m_text = text;
    m_cursorPos = std::clamp(static_cast<int>(m_text.size()), 0, static_cast<int>(m_text.size()));
}

void GlInputPrompt::clear()
{
    m_text.clear();
    m_cursorPos = 0;
}

QString GlInputPrompt::getDisplayText() const
{
    if (m_echoMode == EchoModeEnum::Hidden) {
        return QString(m_text.size(), QChar::fromUcs2('*'));
    }
    return m_text;
}

void GlInputPrompt::backwardHistory()
{
    if (!m_history.atFront()) {
        m_history.backward();
        setText(m_history.value());
    }
}

void GlInputPrompt::forwardHistory()
{
    if (!m_history.atEnd()) {
        m_history.forward();
        if (m_history.atEnd()) {
            clear();
        } else {
            setText(m_history.value());
        }
    }
}

void GlInputPrompt::tabComplete()
{
    if (m_text.isEmpty()) {
        return;
    }
    qsizetype spaceIdx = m_text.lastIndexOf(QChar::fromUcs2(' '));
    QString fragment = (spaceIdx >= 0) ? m_text.mid(spaceIdx + 1) : m_text;
    if (fragment.isEmpty()) {
        return;
    }

    if (auto match = m_tabHistory.nextMatch(fragment)) {
        if (spaceIdx >= 0) {
            m_text = m_text.left(spaceIdx + 1) + *match;
        } else {
            m_text = *match;
        }
        m_cursorPos = static_cast<int>(m_text.size());
    }
}

void GlInputPrompt::cutToClipboard()
{
    copyToClipboard();
    clear();
}

void GlInputPrompt::copyToClipboard() const
{
    if (!m_text.isEmpty() && m_echoMode != EchoModeEnum::Hidden) {
        if (auto *clipboard = QGuiApplication::clipboard()) {
            clipboard->setText(m_text);
        }
    }
}

void GlInputPrompt::pasteFromClipboard()
{
    if (auto *clipboard = QGuiApplication::clipboard()) {
        const QString text = clipboard->text();
        if (!text.isEmpty()) {
            m_text.insert(m_cursorPos, text);
            m_cursorPos += static_cast<int>(text.size());
        }
    }
}

std::optional<QString> GlInputPrompt::handleKeyPress(QKeyEvent *event)
{
    if (!event) {
        return std::nullopt;
    }

    const int key = event->key();
    const Qt::KeyboardModifiers mods = event->modifiers();

    if (mods == Qt::ControlModifier) {
        if (key == Qt::Key_C) {
            copyToClipboard();
            return std::nullopt;
        }
        if (key == Qt::Key_V) {
            pasteFromClipboard();
            return std::nullopt;
        }
        if (key == Qt::Key_X) {
            cutToClipboard();
            return std::nullopt;
        }
    }

    switch (key) {
    case Qt::Key_Return:
    case Qt::Key_Enter: {
        const QString submitted = m_text;
        if (!submitted.isEmpty() && m_echoMode != EchoModeEnum::Hidden) {
            m_history.addInputLine(submitted);
            m_tabHistory.addInputLine(submitted);
        }
        clear();
        return submitted;
    }
    case Qt::Key_Backspace:
        if (m_cursorPos > 0 && !m_text.isEmpty()) {
            m_text.remove(m_cursorPos - 1, 1);
            --m_cursorPos;
        }
        return std::nullopt;

    case Qt::Key_Delete:
        if (m_cursorPos < m_text.size()) {
            m_text.remove(m_cursorPos, 1);
        }
        return std::nullopt;

    case Qt::Key_Left:
        if (m_cursorPos > 0) {
            --m_cursorPos;
        }
        return std::nullopt;

    case Qt::Key_Right:
        if (m_cursorPos < m_text.size()) {
            ++m_cursorPos;
        }
        return std::nullopt;

    case Qt::Key_Home:
        m_cursorPos = 0;
        return std::nullopt;

    case Qt::Key_End:
        m_cursorPos = static_cast<int>(m_text.size());
        return std::nullopt;

    case Qt::Key_Up:
        backwardHistory();
        return std::nullopt;

    case Qt::Key_Down:
        forwardHistory();
        return std::nullopt;

    case Qt::Key_Tab:
        tabComplete();
        return std::nullopt;

    default:
        break;
    }

    const QString keyText = event->text();
    if (!keyText.isEmpty() && keyText[0].isPrint()) {
        m_text.insert(m_cursorPos, keyText);
        m_cursorPos += static_cast<int>(keyText.size());
    }

    return std::nullopt;
}

} // namespace mmgl
