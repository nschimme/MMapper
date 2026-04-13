#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "../global/macros.h"

#include <QDialog>

namespace Ui {
class SetPasswordDialog;
}

class NODISCARD_QOBJECT SetPasswordDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit SetPasswordDialog(QWidget *parent = nullptr);
    ~SetPasswordDialog() final;

    void setAccountName(const QString &name);
    QString accountName() const;

    void setPassword(const QString &password);
    QString password() const;

signals:
    void sig_deleteRequested();

private:
    Ui::SetPasswordDialog *const ui;
};
