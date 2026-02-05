#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#include "../global/macros.h"
#include "Hotkey.h"
#include <QObject>
#include <QStringList>
#include <QFont>

class NODISCARD_QOBJECT InputViewModel final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QFont font READ font NOTIFY appearanceChanged)
    Q_PROPERTY(QString currentText READ currentText WRITE setCurrentText NOTIFY currentTextChanged)

public:
    explicit InputViewModel(QObject *parent = nullptr);

    NODISCARD QFont font() const;
    NODISCARD QString currentText() const { return m_currentText; }
    void setCurrentText(const QString &text);

    void submitInput(const QString &input);
    void nextHistory();
    void prevHistory();
    void tabComplete(const QString &fragment, bool reset);

signals:
    void appearanceChanged();
    void currentTextChanged();
    void sig_sendUserInput(const QString &msg);
    void sig_displayMessage(const QString &msg);
    void sig_showMessage(const QString &msg, int timeout);
    void sig_scrollDisplay(bool pageUp);
    void sig_tabCompletionAvailable(const QString &completion, int fragmentLength);

private:
    void sendCommandWithSeparator(const QString &command);

    QString m_currentText;
    QStringList m_history;
    int m_historyIndex = -1;
    QStringList m_tabDictionary;
    int m_tabIndex = -1;
};
