#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#include "../global/macros.h"
#include <QObject>
#include <QString>

class NODISCARD_QOBJECT PasswordViewModel final : public QObject
{
    Q_OBJECT
public:
    explicit PasswordViewModel(QObject *parent = nullptr);
    void submitPassword(const QString &password);
signals:
    void sig_passwordSubmitted(const QString &password);
};
