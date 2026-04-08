// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#include "grouppage.h"

#include "../configuration/configuration.h"
#include "ui_grouppage.h"

#include <QCheckBox>
#include <QColorDialog>
#include <QPixmap>
#include <QPushButton>

GroupPage::GroupPage(QWidget *const parent, Configuration &config)
    : QWidget(parent)
    , ui(new Ui::GroupPage)
    , m_config(config)
{
    ui->setupUi(this);

    connect(ui->yourColorPushButton, &QPushButton::clicked, this, &GroupPage::slot_chooseColor);

    connect(ui->npcOverrideColorCheckBox, &QCheckBox::stateChanged, this, [this](int checked) {
        m_config.groupManager.npcColorOverride = checked;
        emit sig_changed();
    });
    connect(ui->npcOverrideColorPushButton,
            &QPushButton::clicked,
            this,
            &GroupPage::slot_chooseNpcOverrideColor);

    connect(ui->npcSortBottomCheckbox, &QCheckBox::stateChanged, this, [this](int checked) {
        m_config.groupManager.npcSortBottom = checked;
        emit sig_changed();
    });
    connect(ui->npcHideCheckbox, &QCheckBox::stateChanged, this, [this](int checked) {
        m_config.groupManager.npcHide = checked;
        emit sig_changed();
    });

    slot_loadConfig();
}

GroupPage::~GroupPage()
{
    delete ui;
}

void GroupPage::slot_loadConfig()
{
    const auto &settings = m_config.groupManager;

    QPixmap yourPix(16, 16);
    yourPix.fill(settings.color);
    ui->yourColorPushButton->setIcon(QIcon(yourPix));

    ui->npcOverrideColorCheckBox->setChecked(settings.npcColorOverride);
    QPixmap npcOverridePix(16, 16);
    npcOverridePix.fill(settings.npcColor);
    ui->npcOverrideColorPushButton->setIcon(QIcon(npcOverridePix));

    ui->npcSortBottomCheckbox->setChecked(settings.npcSortBottom);
    ui->npcHideCheckbox->setChecked(settings.npcHide);
}

void GroupPage::slot_chooseColor()
{
    const QColor color = QColorDialog::getColor(m_config.groupManager.color,
                                                this,
                                                tr("Select Your Color"));

    if (color.isValid()) {
        m_config.groupManager.color = color;
        slot_loadConfig();
        emit sig_changed();
    }
}

void GroupPage::slot_chooseNpcOverrideColor()
{
    const QColor color = QColorDialog::getColor(m_config.groupManager.npcColor,
                                                this,
                                                tr("Select NPC Override Color"));

    if (color.isValid()) {
        m_config.groupManager.npcColor = color;
        slot_loadConfig();
        emit sig_changed();
    }
}
