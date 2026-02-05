#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "MumeProtocolViewModel.h"
#include <QWidget>

namespace Ui { class MumeProtocolPage; }

class NODISCARD_QOBJECT MumeProtocolPage final : public QWidget
{
    Q_OBJECT
private:
    Ui::MumeProtocolPage *const ui;
    MumeProtocolViewModel m_viewModel;
public:
    explicit MumeProtocolPage(QWidget *parent);
    ~MumeProtocolPage() final;
public slots:
    void slot_loadConfig();
private:
    void updateUI();
};
