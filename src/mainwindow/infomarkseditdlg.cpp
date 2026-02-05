// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "infomarkseditdlg.h"
#include "ui_infomarkseditdlg.h"
#include "../global/SignalBlocker.h"

InfomarksEditDlg::InfomarksEditDlg(QWidget *parent)
    : QDialog(parent), ui(new Ui::InfomarksEditDlg), m_viewModel(this)
{
    ui->setupUi(this);
    connect(&m_viewModel, &InfoMarksEditViewModel::objectNamesChanged, this, &InfomarksEditDlg::updateUI);
    updateUI();
}

InfomarksEditDlg::~InfomarksEditDlg() = default;

void InfomarksEditDlg::updateUI()
{
}
