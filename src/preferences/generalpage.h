#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "GeneralViewModel.h"
#include "../configuration/PasswordConfig.h"
#include <QWidget>

namespace Ui { class GeneralPage; }

class NODISCARD_QOBJECT GeneralPage final : public QWidget
{
    Q_OBJECT
private:
    Ui::GeneralPage *const ui;
    GeneralViewModel m_viewModel;
    PasswordConfig passCfg;
public:
    explicit GeneralPage(QWidget *parent);
    ~GeneralPage() final;
signals:
    void sig_reloadConfig();
public slots:
    void slot_loadConfig();
private:
    void updateUI();
};
