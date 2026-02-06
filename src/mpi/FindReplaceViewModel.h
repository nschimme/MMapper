#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#include "../global/macros.h"
#include <QObject>
#include <QString>
#include <QTextDocument>

class NODISCARD_QOBJECT FindReplaceViewModel final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString findText READ findText WRITE setFindText NOTIFY findTextChanged)
    Q_PROPERTY(QString replaceText READ replaceText WRITE setReplaceText NOTIFY replaceTextChanged)
    Q_PROPERTY(bool caseSensitive READ caseSensitive WRITE setCaseSensitive NOTIFY caseSensitiveChanged)
    Q_PROPERTY(bool wholeWords READ wholeWords WRITE setWholeWords NOTIFY wholeWordsChanged)
    Q_PROPERTY(bool searchBackward READ searchBackward WRITE setSearchBackward NOTIFY searchBackwardChanged)

public:
    explicit FindReplaceViewModel(bool allowReplace, QObject *parent = nullptr);

    NODISCARD QString findText() const { return m_findText; }
    void setFindText(const QString &t);

    NODISCARD QString replaceText() const { return m_replaceText; }
    void setReplaceText(const QString &t);

    NODISCARD bool caseSensitive() const { return m_caseSensitive; }
    void setCaseSensitive(bool c);

    NODISCARD bool wholeWords() const { return m_wholeWords; }
    void setWholeWords(bool w);

    NODISCARD bool searchBackward() const { return m_searchBackward; }
    void setSearchBackward(bool b);

    void findNext();
    void findPrevious();
    void replaceCurrent();
    void replaceAll();

signals:
    void findTextChanged();
    void replaceTextChanged();
    void caseSensitiveChanged();
    void wholeWordsChanged();
    void searchBackwardChanged();
    void sig_findRequested(const QString &term, QTextDocument::FindFlags flags);
    void sig_replaceCurrentRequested(const QString &findTerm, const QString &replaceTerm, QTextDocument::FindFlags flags);
    void sig_replaceAllRequested(const QString &findTerm, const QString &replaceTerm, QTextDocument::FindFlags flags);

private:
    NODISCARD QTextDocument::FindFlags getFlags() const;
    bool m_allowReplace;
    QString m_findText;
    QString m_replaceText;
    bool m_caseSensitive = false;
    bool m_wholeWords = false;
    bool m_searchBackward = false;
};
