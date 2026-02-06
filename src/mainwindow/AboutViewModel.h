#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#include "../global/macros.h"
#include <QObject>
#include <QString>

class NODISCARD_QOBJECT AboutViewModel final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString aboutHtml READ aboutHtml CONSTANT)
    Q_PROPERTY(QString authorsHtml READ authorsHtml CONSTANT)

public:
    explicit AboutViewModel(QObject *parent = nullptr);

    NODISCARD QString aboutHtml() const;
    NODISCARD QString authorsHtml() const;
};
