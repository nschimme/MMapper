// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "clientpage.h"
#include "ui_clientpage.h"
#include "../global/SignalBlocker.h"
#include <QFontDialog>
#include <QColorDialog>

ClientPage::ClientPage(QWidget *parent)
    : QWidget(parent), ui(new Ui::ClientPage)
{
    ui->setupUi(this);
    connect(&m_viewModel, &ClientPageViewModel::settingsChanged, this, &ClientPage::updateUI);

    connect(ui->fontPushButton, &QPushButton::clicked, this, &ClientPage::slot_onChangeFont);
    connect(ui->bgColorPushButton, &QPushButton::clicked, this, &ClientPage::slot_onChangeBgColor);
    connect(ui->fgColorPushButton, &QPushButton::clicked, this, &ClientPage::slot_onChangeFgColor);

    connect(ui->columnsSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), &m_viewModel, &ClientPageViewModel::setColumns);
    connect(ui->rowsSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), &m_viewModel, &ClientPageViewModel::setRows);

    updateUI();
}

ClientPage::~ClientPage() = default;

void ClientPage::updateUI()
{
    SignalBlocker sb1(*ui->columnsSpinBox);
    SignalBlocker sb2(*ui->rowsSpinBox);

    ui->exampleLabel->setText(m_viewModel.font().family() + " " + QString::number(m_viewModel.font().pointSize()));
    ui->columnsSpinBox->setValue(m_viewModel.columns());
    ui->rowsSpinBox->setValue(m_viewModel.rows());
}

void ClientPage::slot_onChangeFont()
{
    bool ok;
    QFont font = QFontDialog::getFont(&ok, m_viewModel.font(), this);
    if (ok) m_viewModel.setFont(font);
}

void ClientPage::slot_onChangeBgColor()
{
    QColor color = QColorDialog::getColor(m_viewModel.backgroundColor(), this);
    if (color.isValid()) m_viewModel.setBackgroundColor(color);
}

void ClientPage::slot_onChangeFgColor()
{
    QColor color = QColorDialog::getColor(m_viewModel.foregroundColor(), this);
    if (color.isValid()) m_viewModel.setForegroundColor(color);
}

void ClientPage::slot_loadConfig() { m_viewModel.loadConfig(); }
