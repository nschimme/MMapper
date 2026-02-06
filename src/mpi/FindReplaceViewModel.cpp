// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#include "FindReplaceViewModel.h"

FindReplaceViewModel::FindReplaceViewModel(bool allowReplace, QObject *parent)
    : QObject(parent), m_allowReplace(allowReplace) {}

void FindReplaceViewModel::setFindText(const QString &t) { if (m_findText != t) { m_findText = t; emit findTextChanged(); } }
void FindReplaceViewModel::setReplaceText(const QString &t) { if (m_replaceText != t) { m_replaceText = t; emit replaceTextChanged(); } }
void FindReplaceViewModel::setCaseSensitive(bool c) { if (m_caseSensitive != c) { m_caseSensitive = c; emit caseSensitiveChanged(); } }
void FindReplaceViewModel::setWholeWords(bool w) { if (m_wholeWords != w) { m_wholeWords = w; emit wholeWordsChanged(); } }
void FindReplaceViewModel::setSearchBackward(bool b) { if (m_searchBackward != b) { m_searchBackward = b; emit searchBackwardChanged(); } }

QTextDocument::FindFlags FindReplaceViewModel::getFlags() const {
    QTextDocument::FindFlags flags;
    if (m_caseSensitive) flags |= QTextDocument::FindCaseSensitively;
    if (m_wholeWords) flags |= QTextDocument::FindWholeWords;
    if (m_searchBackward) flags |= QTextDocument::FindBackward;
    return flags;
}

void FindReplaceViewModel::findNext() { emit sig_findRequested(m_findText, getFlags() & ~QTextDocument::FindBackward); }
void FindReplaceViewModel::findPrevious() { emit sig_findRequested(m_findText, getFlags() | QTextDocument::FindBackward); }
void FindReplaceViewModel::replaceCurrent() { if (m_allowReplace) emit sig_replaceCurrentRequested(m_findText, m_replaceText, getFlags()); }
void FindReplaceViewModel::replaceAll() { if (m_allowReplace) emit sig_replaceAllRequested(m_findText, m_replaceText, getFlags()); }
