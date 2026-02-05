#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#include "../global/macros.h"
#include <QObject>
#include <QString>

class NODISCARD_QOBJECT AnsiColorViewModel final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString ansiString READ ansiString WRITE setAnsiString NOTIFY ansiStringChanged)

public:
    explicit AnsiColorViewModel(QObject *parent = nullptr);

    NODISCARD QString ansiString() const { return m_ansiString; }
    void setAnsiString(const QString &v);

signals:
    void ansiStringChanged();

private:
    QString m_ansiString;
};
