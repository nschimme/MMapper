// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "AnsiColorDialog.h"
#include "ui_AnsiColorDialog.h"

AnsiColorDialog::AnsiColorDialog(const QString &ansi, QWidget *parent)
    : QDialog(parent), m_ui(new Ui::AnsiColorDialog)
{
    m_ui->setupUi(this);
    m_viewModel.setAnsiString(ansi);
    connect(&m_viewModel, &AnsiColorViewModel::ansiStringChanged, this, &AnsiColorDialog::updateUI);
    updateUI();
}
AnsiColorDialog::~AnsiColorDialog() = default;
void AnsiColorDialog::getColor(const QString &ansi, QWidget *p, std::function<void(QString)> cb) {
    auto *d = new AnsiColorDialog(ansi, p);
    connect(d, &QDialog::accepted, [d, cb]() { cb(d->m_viewModel.ansiString()); });
    d->setAttribute(Qt::WA_DeleteOnClose); d->open();
}
void AnsiColorDialog::updateUI() { /* Placeholder */ }
