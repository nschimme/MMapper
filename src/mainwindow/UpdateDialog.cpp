// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "UpdateDialog.h"
#include <QGridLayout>
#include <QPushButton>
#include <QDesktopServices>

UpdateDialog::UpdateDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("MMapper Updater");
    m_text = new QLabel(this);
    m_buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    m_buttonBox->button(QDialogButtonBox::Ok)->setText(tr("&Upgrade"));

    QGridLayout *l = new QGridLayout(this);
    l->addWidget(m_text);
    l->addWidget(m_buttonBox);

    connect(&m_viewModel, &UpdateViewModel::statusChanged, this, &UpdateDialog::updateUI);
    connect(&m_viewModel, &UpdateViewModel::sig_showUpdateDialog, this, &QDialog::show);
    connect(m_buttonBox, &QDialogButtonBox::accepted, this, [this]() {
        QDesktopServices::openUrl(QUrl("https://github.com/MUME/MMapper/releases"));
        close();
    });
    connect(m_buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    updateUI();
}

void UpdateDialog::open() { m_viewModel.checkUpdates(); }

void UpdateDialog::updateUI() {
    m_text->setText(m_viewModel.statusText());
    m_buttonBox->button(QDialogButtonBox::Ok)->setEnabled(m_viewModel.upgradeButtonEnabled());
}
