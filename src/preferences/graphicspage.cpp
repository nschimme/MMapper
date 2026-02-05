// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "graphicspage.h"
#include "ui_graphicspage.h"
#include "AdvancedGraphics.h"
#include "../global/SignalBlocker.h"

GraphicsPage::GraphicsPage(QWidget *parent)
    : QWidget(parent), ui(new Ui::GraphicsPage)
{
    ui->setupUi(this);
    connect(&m_viewModel, &GraphicsViewModel::settingsChanged, this, &GraphicsPage::updateUI);

    connect(ui->drawNeedsUpdate, &QCheckBox::toggled, &m_viewModel, &GraphicsViewModel::setDrawNeedsUpdate);
    connect(ui->drawNotMappedExits, &QCheckBox::toggled, &m_viewModel, &GraphicsViewModel::setDrawNotMappedExits);
    connect(ui->drawDoorNames, &QCheckBox::toggled, &m_viewModel, &GraphicsViewModel::setDrawDoorNames);
    connect(ui->drawUpperLayersTextured, &QCheckBox::toggled, &m_viewModel, &GraphicsViewModel::setDrawUpperLayersTextured);
    connect(ui->resourceLineEdit, &QLineEdit::textChanged, &m_viewModel, &GraphicsViewModel::setResourceDir);

    m_advanced = std::make_unique<AdvancedGraphicsGroupBox>(deref(ui->groupBox_Advanced));
    connect(m_advanced.get(), &AdvancedGraphicsGroupBox::sig_graphicsSettingsChanged, this, &GraphicsPage::sig_graphicsSettingsChanged);

    updateUI();
}

GraphicsPage::~GraphicsPage() = default;

void GraphicsPage::updateUI()
{
    SignalBlocker sb1(*ui->drawNeedsUpdate);
    SignalBlocker sb2(*ui->drawNotMappedExits);
    SignalBlocker sb3(*ui->drawDoorNames);
    SignalBlocker sb4(*ui->drawUpperLayersTextured);
    SignalBlocker sb5(*ui->resourceLineEdit);

    ui->drawNeedsUpdate->setChecked(m_viewModel.drawNeedsUpdate());
    ui->drawNotMappedExits->setChecked(m_viewModel.drawNotMappedExits());
    ui->drawDoorNames->setChecked(m_viewModel.drawDoorNames());
    ui->drawUpperLayersTextured->setChecked(m_viewModel.drawUpperLayersTextured());
    ui->resourceLineEdit->setText(m_viewModel.resourceDir());
}

void GraphicsPage::slot_loadConfig()
{
    m_viewModel.loadConfig();
}
