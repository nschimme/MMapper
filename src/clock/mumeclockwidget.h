#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "MumeClockViewModel.h"
#include <QWidget>
#include <memory>

namespace Ui { class MumeClockWidget; }

class NODISCARD_QOBJECT MumeClockWidget final : public QWidget
{
    Q_OBJECT
private:
    std::unique_ptr<Ui::MumeClockWidget> ui;
    MumeClockViewModel m_viewModel;

public:
    explicit MumeClockWidget(QWidget *parent = nullptr);
    ~MumeClockWidget() final;

private slots:
    void updateUI();
};
