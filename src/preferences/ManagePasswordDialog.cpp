// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "ManagePasswordDialog.h"

#include "ui_ManagePasswordDialog.h"

#include <QLineEdit>
#include <QPushButton>

ManagePasswordDialog::ManagePasswordDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::ManagePasswordDialog)
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

ManagePasswordDialog::~ManagePasswordDialog()
{
    delete ui;
}

void ManagePasswordDialog::setAccountName(const QString &name)
{
    ui->accountName->setText(name);
}

QString ManagePasswordDialog::accountName() const
{
    return ui->accountName->text();
}

void ManagePasswordDialog::setPassword(const QString &password)
{
    ui->accountPassword->setText(password);
}

QString ManagePasswordDialog::password() const
{
    return ui->accountPassword->text();
}
