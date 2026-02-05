#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "AutoLogPageViewModel.h"
#include <QWidget>

namespace Ui {
class AutoLogPage;
}

class NODISCARD_QOBJECT AutoLogPage final : public QWidget
{
    Q_OBJECT

private:
    Ui::AutoLogPage *const ui;
    AutoLogPageViewModel m_viewModel;

public:
    explicit AutoLogPage(QWidget *parent);
    ~AutoLogPage() final;

public slots:
    void slot_loadConfig();

private:
    void updateUI();
};
