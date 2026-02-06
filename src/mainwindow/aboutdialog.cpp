// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "aboutdialog.h"

AboutDialog::AboutDialog(QWidget *parent)
    : QDialog(parent), ui(new Ui::AboutDialog)
{
    ui->setupUi(this);
    setWindowIcon(QIcon(":/icons/m.png"));

    ui->pixmapLabel->setPixmap(QPixmap(":/pixmaps/splash.png"));
    ui->aboutText->setText(m_viewModel.aboutHtml());
    ui->authorsView->setHtml(m_viewModel.authorsHtml());

    adjustSize();
}
