#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "XPStatusViewModel.h"
#include <QPushButton>

class NODISCARD_QOBJECT XPStatusWidget final : public QPushButton
{
    Q_OBJECT
private:
    XPStatusViewModel m_viewModel;

public:
    explicit XPStatusWidget(QWidget *parent = nullptr);
    ~XPStatusWidget() final = default;

private slots:
    void updateUI();
};
