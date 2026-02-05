// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#include "InputViewModel.h"
#include "../configuration/configuration.h"
#include <QRegularExpression>

InputViewModel::InputViewModel(QObject *parent) : QObject(parent) {}

QFont InputViewModel::font() const { QFont f; f.fromString(getConfig().integratedClient.font); return f; }
void InputViewModel::setCurrentText(const QString &t) { if (m_currentText != t) { m_currentText = t; emit currentTextChanged(); } }

void InputViewModel::submitInput(const QString &input) {
    if (input.isEmpty()) return;
    if (m_history.isEmpty() || m_history.first() != input) {
        m_history.prepend(input);
        if (m_history.size() > getConfig().integratedClient.linesOfInputHistory) m_history.removeLast();
    }
    m_historyIndex = -1;

    QStringList words = input.split(QRegularExpression(R"(\s+)"), Qt::SkipEmptyParts);
    for (const auto &w : words) if (w.length() > 3) {
        m_tabDictionary.removeAll(w); m_tabDictionary.prepend(w);
        if (m_tabDictionary.size() > getConfig().integratedClient.tabCompletionDictionarySize) m_tabDictionary.removeLast();
    }

    sendCommandWithSeparator(input);
}

void InputViewModel::sendCommandWithSeparator(const QString &command) {
    const auto &s = getConfig().integratedClient;
    if (s.useCommandSeparator && !s.commandSeparator.isEmpty()) {
        const QString sep = s.commandSeparator;
        const QRegularExpression r(QString("(?<!\\\\)%1").arg(QRegularExpression::escape(sep)));
        QStringList cmds = command.split(r);
        for (auto &cmd : cmds) { cmd.replace("\\" + sep, sep); emit sig_sendUserInput(cmd); }
    } else emit sig_sendUserInput(command);
}

void InputViewModel::nextHistory() {
    if (m_historyIndex > 0) { m_historyIndex--; setCurrentText(m_history[m_historyIndex]); }
    else if (m_historyIndex == 0) { m_historyIndex = -1; setCurrentText(""); }
}

void InputViewModel::prevHistory() {
    if (m_historyIndex + 1 < m_history.size()) { m_historyIndex++; setCurrentText(m_history[m_historyIndex]); }
    else emit sig_showMessage("Reached end of input history", 1000);
}

void InputViewModel::tabComplete(const QString &fragment, bool reset) {
    if (reset) m_tabIndex = -1;
    for (int i = m_tabIndex + 1; i < m_tabDictionary.size(); ++i) {
        if (m_tabDictionary[i].startsWith(fragment)) {
            m_tabIndex = i; emit sig_tabCompletionAvailable(m_tabDictionary[i], fragment.length()); return;
        }
    }
    m_tabIndex = -1; // Reset to loop around
}
