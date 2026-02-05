#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "ParserPageViewModel.h"
#include <QWidget>

namespace Ui {
class ParserPage;
}

class NODISCARD_QOBJECT ParserPage final : public QWidget
{
    Q_OBJECT

private:
    Ui::ParserPage *const ui;
    ParserPageViewModel m_viewModel;

public:
    explicit ParserPage(QWidget *parent);
    ~ParserPage() final;

public slots:
    void slot_loadConfig();

private:
    void updateUI();
};
