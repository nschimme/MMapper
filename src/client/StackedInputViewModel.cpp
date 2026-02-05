// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#include "StackedInputViewModel.h"

StackedInputViewModel::StackedInputViewModel(QObject *parent)
    : QObject(parent)
    , m_inputViewModel(new InputViewModel(this))
    , m_passwordViewModel(new PasswordViewModel(this))
{
}

void StackedInputViewModel::setPasswordMode(bool enabled)
{
    int newIndex = enabled ? 1 : 0;
    if (m_currentIndex != newIndex) {
        m_currentIndex = newIndex;
        emit currentIndexChanged();
    }
}
