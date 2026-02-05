#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#include "../global/macros.h"
#include <QObject>
#include <QString>

class NODISCARD_QOBJECT XPStatusViewModel final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString text READ text NOTIFY textChanged)

public:
    explicit XPStatusViewModel(QObject *parent = nullptr);

    NODISCARD QString text() const { return m_text; }
    void setText(const QString &t);

signals:
    void textChanged();

private:
    QString m_text;
};
