// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2024 The MMapper Authors

#include "PreviewWidget.h"

PreviewWidget::PreviewWidget(QWidget *parent)
    : QTextEdit(parent), helper(*this)
{
    setReadOnly(true);
    connect(&m_viewModel, &PreviewViewModel::textChanged, this, &PreviewWidget::updateUI);
}

void PreviewWidget::init(const QFont &font) {
    setFont(font);
    helper.init();
}

void PreviewWidget::updateUI() {
    helper.displayText(m_viewModel.text());
}
