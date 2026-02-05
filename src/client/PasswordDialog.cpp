// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "PasswordDialog.h"
#include <QVBoxLayout>
#include <QLabel>

PasswordDialog::PasswordDialog(QWidget *const parent)
    : QDialog(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(tr("Password:"), this));
    m_passwordLineEdit = new QLineEdit(this);
    m_passwordLineEdit->setEchoMode(QLineEdit::Password);
    layout->addWidget(m_passwordLineEdit);
    connect(m_passwordLineEdit, &QLineEdit::returnPressed, this, &PasswordDialog::accept);
}

void PasswordDialog::accept()
{
    emit sig_passwordSubmitted(m_passwordLineEdit->text());
    m_passwordLineEdit->clear();
    QDialog::accept();
}

bool PasswordDialog::focusNextPrevChild(bool) { return false; }
