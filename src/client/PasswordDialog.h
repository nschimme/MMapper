#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
#include "../global/macros.h"
#include <QDialog>
#include <QLineEdit>

class NODISCARD_QOBJECT PasswordDialog final : public QDialog
{
    Q_OBJECT
public:
    explicit PasswordDialog(QWidget *parent = nullptr);
protected:
    NODISCARD bool focusNextPrevChild(bool next) override;
private slots:
    void accept() override;
signals:
    void sig_passwordSubmitted(const QString &password);
private:
    QLineEdit *m_passwordLineEdit;
};
