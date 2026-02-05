#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#include "../global/macros.h"
#include <QObject>
#include <QString>

class NODISCARD_QOBJECT MumeClockViewModel final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString timeString READ timeString NOTIFY clockChanged)
    Q_PROPERTY(QString seasonString READ seasonString NOTIFY clockChanged)

public:
    explicit MumeClockViewModel(QObject *parent = nullptr);

    NODISCARD QString timeString() const { return m_timeString; }
    NODISCARD QString seasonString() const { return m_seasonString; }

    void update(const QString &time, const QString &season);

signals:
    void clockChanged();

private:
    QString m_timeString;
    QString m_seasonString;
};
