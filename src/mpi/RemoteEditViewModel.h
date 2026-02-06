#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#include "../global/macros.h"
#include <QObject>
#include <QString>

class NODISCARD_QOBJECT RemoteEditViewModel final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString text READ text WRITE setText NOTIFY textChanged)
    Q_PROPERTY(QString title READ title NOTIFY titleChanged)
    Q_PROPERTY(bool isEditSession READ isEditSession CONSTANT)

public:
    explicit RemoteEditViewModel(bool editSession, const QString &title, const QString &body, QObject *parent = nullptr);

    NODISCARD QString text() const { return m_text; }
    void setText(const QString &t);

    NODISCARD QString title() const { return m_title; }
    NODISCARD bool isEditSession() const { return m_editSession; }

    void submit();
    void cancel();

signals:
    void textChanged();
    void titleChanged();
    void sig_save(const QString &text);
    void sig_cancel();

private:
    bool m_editSession;
    QString m_title;
    QString m_text;
};
