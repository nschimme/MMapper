#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "ClientPageViewModel.h"
#include <QWidget>

namespace Ui { class ClientPage; }

class NODISCARD_QOBJECT ClientPage final : public QWidget
{
    Q_OBJECT
private:
    Ui::ClientPage *const ui;
    ClientPageViewModel m_viewModel;
public:
    explicit ClientPage(QWidget *parent);
    ~ClientPage() final;
public slots:
    void slot_loadConfig();
    void slot_onChangeFont();
    void slot_onChangeBgColor();
    void slot_onChangeFgColor();
private:
    void updateUI();
};
