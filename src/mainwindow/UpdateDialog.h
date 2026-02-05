#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "UpdateViewModel.h"
#include <QDialog>
#include <QLabel>
#include <QDialogButtonBox>

class NODISCARD_QOBJECT UpdateDialog final : public QDialog
{
    Q_OBJECT
private:
    UpdateViewModel m_viewModel;
    QLabel *m_text;
    QDialogButtonBox *m_buttonBox;

public:
    explicit UpdateDialog(QWidget *parent = nullptr);
    ~UpdateDialog() final = default;

public slots:
    void open() override;

private slots:
    void updateUI();
};
