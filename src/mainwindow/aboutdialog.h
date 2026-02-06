#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "AboutViewModel.h"
#include "ui_aboutdialog.h"
#include <QDialog>
#include <memory>

class NODISCARD_QOBJECT AboutDialog : public QDialog
{
    Q_OBJECT
private:
    std::unique_ptr<Ui::AboutDialog> ui;
    AboutViewModel m_viewModel;

public:
    explicit AboutDialog(QWidget *parent);
    ~AboutDialog() final = default;
};
