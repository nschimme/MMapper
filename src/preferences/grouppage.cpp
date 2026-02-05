// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "grouppage.h"
#include "ui_grouppage.h"
#include "../global/SignalBlocker.h"
#include <QColorDialog>

GroupPage::GroupPage(QWidget *parent)
    : QWidget(parent), ui(new Ui::GroupPage)
{
    ui->setupUi(this);
    connect(&m_viewModel, &GroupPageViewModel::settingsChanged, this, &GroupPage::updateUI);
    connect(&m_viewModel, &GroupPageViewModel::settingsChanged, this, &GroupPage::sig_groupSettingsChanged);

    connect(ui->yourColorPushButton, &QPushButton::clicked, this, &GroupPage::slot_chooseColor);
    connect(ui->npcOverrideColorPushButton, &QPushButton::clicked, this, &GroupPage::slot_chooseNpcOverrideColor);
    connect(ui->npcOverrideColorCheckBox, &QCheckBox::toggled, &m_viewModel, &GroupPageViewModel::setNpcColorOverride);
    connect(ui->npcSortBottomCheckbox, &QCheckBox::toggled, &m_viewModel, &GroupPageViewModel::setNpcSortBottom);
    connect(ui->npcHideCheckbox, &QCheckBox::toggled, &m_viewModel, &GroupPageViewModel::setNpcHide);

    updateUI();
}

GroupPage::~GroupPage() = default;

void GroupPage::updateUI()
{
    SignalBlocker sb1(*ui->npcOverrideColorCheckBox);
    SignalBlocker sb2(*ui->npcSortBottomCheckbox);
    SignalBlocker sb3(*ui->npcHideCheckbox);

    ui->npcOverrideColorCheckBox->setChecked(m_viewModel.npcColorOverride());
    ui->npcSortBottomCheckbox->setChecked(m_viewModel.npcSortBottom());
    ui->npcHideCheckbox->setChecked(m_viewModel.npcHide());
}

void GroupPage::slot_loadConfig()
{
    m_viewModel.loadConfig();
}

void GroupPage::slot_chooseColor()
{
    QColor color = QColorDialog::getColor(m_viewModel.color(), this);
    if (color.isValid()) m_viewModel.setColor(color);
}

void GroupPage::slot_chooseNpcOverrideColor()
{
    QColor color = QColorDialog::getColor(m_viewModel.npcColor(), this);
    if (color.isValid()) m_viewModel.setNpcColor(color);
}
