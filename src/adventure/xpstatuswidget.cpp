// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "xpstatuswidget.h"

XPStatusWidget::XPStatusWidget(QWidget *parent)
    : QPushButton(parent)
{
    connect(&m_viewModel, &XPStatusViewModel::textChanged, this, &XPStatusWidget::updateUI);
    updateUI();
}

void XPStatusWidget::updateUI() {
    setText(m_viewModel.text());
}
