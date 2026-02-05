// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "roomeditattrdlg.h"
#include "ui_roomeditattrdlg.h"
#include "../global/SignalBlocker.h"

RoomEditAttrDlg::RoomEditAttrDlg(QWidget *parent)
    : QDialog(parent), ui(new Ui::RoomEditAttrDlg), m_viewModel(this)
{
    ui->setupUi(this);
    connect(&m_viewModel, &RoomEditAttrViewModel::roomNamesChanged, this, &RoomEditAttrDlg::updateUI);
    updateUI();
}

RoomEditAttrDlg::~RoomEditAttrDlg() = default;

void RoomEditAttrDlg::updateUI()
{
}
