#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#include "InputViewModel.h"
#include "PasswordViewModel.h"
#include "../global/macros.h"
#include <QObject>

class NODISCARD_QOBJECT StackedInputViewModel final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int currentIndex READ currentIndex NOTIFY currentIndexChanged)

public:
    explicit StackedInputViewModel(QObject *parent = nullptr);

    NODISCARD int currentIndex() const { return m_currentIndex; }
    void setPasswordMode(bool enabled);

    InputViewModel* inputViewModel() const { return m_inputViewModel; }
    PasswordViewModel* passwordViewModel() const { return m_passwordViewModel; }

signals:
    void currentIndexChanged();

private:
    int m_currentIndex = 0;
    InputViewModel* m_inputViewModel;
    PasswordViewModel* m_passwordViewModel;
};
