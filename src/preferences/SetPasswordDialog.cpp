// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "SetPasswordDialog.h"
#include "ui_SetPasswordDialog.h"

#include <QLineEdit>
#include <QPushButton>

SetPasswordDialog::SetPasswordDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::SetPasswordDialog)
{
    ui->setupUi(this);

    connect(ui->showPassword, &QToolButton::toggled, this, [this](bool checked) {
        ui->accountPassword->setEchoMode(checked ? QLineEdit::Normal : QLineEdit::Password);
    });

    connect(ui->deleteButton, &QPushButton::clicked, this, [this]() {
        emit sig_deleteRequested();
        reject();
    });
}

SetPasswordDialog::~SetPasswordDialog()
{
    delete ui;
}

void SetPasswordDialog::setAccountName(const QString &name)
{
    ui->accountName->setText(name);
}

QString SetPasswordDialog::accountName() const
{
    return ui->accountName->text();
}

void SetPasswordDialog::setPassword(const QString &password)
{
    ui->accountPassword->setText(password);
}

QString SetPasswordDialog::password() const
{
    return ui->accountPassword->text();
}
