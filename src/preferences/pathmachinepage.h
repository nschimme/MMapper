#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "PathMachinePageViewModel.h"
#include <QWidget>

namespace Ui {
class PathmachinePage;
}

class NODISCARD_QOBJECT PathmachinePage final : public QWidget
{
    Q_OBJECT

private:
    Ui::PathmachinePage *const ui;
    PathMachinePageViewModel m_viewModel;

public:
    explicit PathmachinePage(QWidget *parent);
    ~PathmachinePage() final;

public slots:
    void slot_loadConfig();

private:
    void updateUI();
};
