// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#include "grouppage.h"

#include "../configuration/configuration.h"
#include "../global/SignalBlocker.h"
#include "ui_grouppage.h"

#include <QCheckBox>
#include <QColorDialog>
#include <QPixmap>
#include <QPushButton>

GroupPage::GroupPage(QWidget *const parent)
    : QWidget(parent)
    , ui(new Ui::GroupPage)
{
    ui->setupUi(this);

    connect(ui->yourColorPushButton, &QPushButton::clicked, this, &GroupPage::slot_chooseColor);

    connect(ui->npcOverrideColorCheckBox, &QCheckBox::stateChanged, this, [this](int checked) {
        setConfig().groupManager.npcColorOverride.set(checked);
    });
    connect(ui->npcOverrideColorPushButton,
            &QPushButton::clicked,
            this,
            &GroupPage::slot_chooseNpcOverrideColor);

    connect(ui->npcSortBottomCheckbox, &QCheckBox::stateChanged, this, [this](int checked) {
        setConfig().groupManager.npcSortBottom.set(checked);
    });
    connect(ui->npcHideCheckbox, &QCheckBox::stateChanged, this, [this](int checked) {
        setConfig().groupManager.npcHide.set(checked);
    });

    auto &group = setConfig().groupManager;
    group.color.registerChangeCallback(m_lifetime, [this]() { slot_loadConfig(); });
    group.npcColor.registerChangeCallback(m_lifetime, [this]() { slot_loadConfig(); });
    group.npcColorOverride.registerChangeCallback(m_lifetime, [this]() { slot_loadConfig(); });
    group.npcSortBottom.registerChangeCallback(m_lifetime, [this]() { slot_loadConfig(); });
    group.npcHide.registerChangeCallback(m_lifetime, [this]() { slot_loadConfig(); });

    slot_loadConfig();
}

GroupPage::~GroupPage()
{
    delete ui;
}

void GroupPage::slot_loadConfig()
{
    const auto &settings = getConfig().groupManager;

    SignalBlocker b1(*ui->npcOverrideColorCheckBox);
    SignalBlocker b2(*ui->npcSortBottomCheckbox);
    SignalBlocker b3(*ui->npcHideCheckbox);

    QPixmap yourPix(16, 16);
    yourPix.fill(settings.color.get());
    ui->yourColorPushButton->setIcon(QIcon(yourPix));

    ui->npcOverrideColorCheckBox->setChecked(settings.npcColorOverride.get());
    QPixmap npcOverridePix(16, 16);
    npcOverridePix.fill(settings.npcColor.get());
    ui->npcOverrideColorPushButton->setIcon(QIcon(npcOverridePix));

    ui->npcSortBottomCheckbox->setChecked(settings.npcSortBottom.get());
    ui->npcHideCheckbox->setChecked(settings.npcHide.get());
}

void GroupPage::slot_chooseColor()
{
    const QColor color = QColorDialog::getColor(getConfig().groupManager.color.get(),
                                                this,
                                                tr("Select Your Color"));

    if (color.isValid()) {
        setConfig().groupManager.color.set(color);
        slot_loadConfig();
    }
}

void GroupPage::slot_chooseNpcOverrideColor()
{
    const QColor color = QColorDialog::getColor(getConfig().groupManager.npcColor.get(),
                                                this,
                                                tr("Select NPC Override Color"));

    if (color.isValid()) {
        setConfig().groupManager.npcColor.set(color);
        slot_loadConfig();
    }
}
