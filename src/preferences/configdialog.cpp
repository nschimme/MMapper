// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "configdialog.h"
#include "ui_configdialog.h"
#include "../global/SignalBlocker.h"

ConfigDialog::ConfigDialog(QWidget *parent)
    : QDialog(parent), ui(new Ui::ConfigDialog), m_viewModel(this)
{
    ui->setupUi(this);

    connect(ui->contentsWidget, &QListWidget::currentItemChanged, this, &ConfigDialog::slot_changePage);
    connect(&m_viewModel, &ConfigViewModel::currentPageIndexChanged, this, &ConfigDialog::updateUI);

    updateUI();
}

ConfigDialog::~ConfigDialog() = default;

void ConfigDialog::slot_changePage(QListWidgetItem *current, QListWidgetItem *)
{
    if (current) {
        int index = ui->contentsWidget->row(current);
        m_viewModel.setCurrentPageIndex(index);
    }
}

void ConfigDialog::updateUI()
{
    ui->pagesWidget->setCurrentIndex(m_viewModel.currentPageIndex());
    SignalBlocker sb(*ui->contentsWidget);
    ui->contentsWidget->setCurrentRow(m_viewModel.currentPageIndex());
}
