// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "stackedinputwidget.h"
#include "inputwidget.h"
#include "PasswordDialog.h"

StackedInputWidget::StackedInputWidget(QWidget *const parent)
    : QStackedWidget(parent), m_viewModel(this)
{
    m_inputWidget = new InputWidget(m_viewModel.inputViewModel(), this);
    m_passwordDialog = new PasswordDialog(this);
    addWidget(m_inputWidget);
    addWidget(m_passwordDialog);
    setFocusProxy(m_inputWidget);
    connect(&m_viewModel, &StackedInputViewModel::currentIndexChanged, this, [this]() {
        setCurrentIndex(m_viewModel.currentIndex());
    });
    connect(m_passwordDialog, &PasswordDialog::sig_passwordSubmitted, m_viewModel.passwordViewModel(), &PasswordViewModel::submitPassword);
    setCurrentIndex(m_viewModel.currentIndex());
}

StackedInputWidget::~StackedInputWidget() = default;

void StackedInputWidget::slot_cut() { if (currentIndex() == 0) m_inputWidget->cut(); }
void StackedInputWidget::slot_copy() { if (currentIndex() == 0) m_inputWidget->copy(); }
void StackedInputWidget::slot_paste() { if (currentIndex() == 0) m_inputWidget->paste(); }
