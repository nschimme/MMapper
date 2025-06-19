// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#include "grouppage.h"

#include "../configuration/configuration.h"
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

    // Initialize m_configLifetime and register callbacks
    m_configLifetime.disconnectAll(); // Ensure clean slate for connections
    Configuration &config = getConfig();

    config.groupManager.registerChangeCallback(m_configLifetime, [this]() {
        // When groupManager settings change, this page should reflect those changes
        // and signal that its relevant settings might have updated.
        // slot_loadConfig(); // Optionally refresh UI from config first
        emit sig_groupSettingsChanged();
    });

    connect(ui->yourColorPushButton, &QPushButton::clicked, this, &GroupPage::slot_chooseColor);

    connect(ui->npcOverrideColorCheckBox, &QCheckBox::stateChanged, this, [this](int checked) {
        setConfig().groupManager.setNpcColorOverride(checked); // Use setter
        // emit sig_groupSettingsChanged(); // Removed
    });
    connect(ui->npcOverrideColorPushButton,
            &QPushButton::clicked,
            this,
            &GroupPage::slot_chooseNpcOverrideColor);

    connect(ui->npcSortBottomCheckbox, &QCheckBox::stateChanged, this, [this](int checked) {
        setConfig().groupManager.setNpcSortBottom(checked); // Use setter
        // emit sig_groupSettingsChanged(); // Removed
    });
    connect(ui->npcHideCheckbox, &QCheckBox::stateChanged, this, [this](int checked) {
        setConfig().groupManager.setNpcHide(checked); // Use setter
        // emit sig_groupSettingsChanged(); // Removed
    });

    slot_loadConfig();
}

GroupPage::~GroupPage()
{
    delete ui;
}

void GroupPage::slot_loadConfig()
{
    const auto &settings = getConfig().groupManager;

    QPixmap yourPix(16, 16);
    yourPix.fill(settings.getColor()); // Use getter
    ui->yourColorPushButton->setIcon(QIcon(yourPix));

    ui->npcOverrideColorCheckBox->setChecked(settings.getNpcColorOverride()); // Use getter
    QPixmap npcOverridePix(16, 16);
    npcOverridePix.fill(settings.getNpcColor()); // Use getter
    ui->npcOverrideColorPushButton->setIcon(QIcon(npcOverridePix));

    ui->npcSortBottomCheckbox->setChecked(settings.getNpcSortBottom()); // Use getter
    ui->npcHideCheckbox->setChecked(settings.getNpcHide()); // Use getter
}

void GroupPage::slot_chooseColor()
{
    const QColor color = QColorDialog::getColor(getConfig().groupManager.getColor(), // Use getter
                                                this,
                                                tr("Select Your Color"));

    if (color.isValid()) {
        setConfig().groupManager.setColor(color); // Use setter
        // slot_loadConfig(); // Re-sync UI. The monitor callback will handle the signal.
                           // If direct UI feedback (icon color) is needed immediately without waiting for signal loop,
                           // then slot_loadConfig() or direct UI update here is fine.
                           // For now, assume monitor handles eventual consistency.
                           // Let's keep slot_loadConfig() to ensure button icon updates immediately.
        slot_loadConfig();
        // emit sig_groupSettingsChanged(); // Removed
    }
}

void GroupPage::slot_chooseNpcOverrideColor()
{
    const QColor color = QColorDialog::getColor(getConfig().groupManager.getNpcColor(), // Use getter
                                                this,
                                                tr("Select NPC Override Color"));

    if (color.isValid()) {
        setConfig().groupManager.setNpcColor(color); // Use setter
        // slot_loadConfig(); // Similar to above.
        slot_loadConfig();
        // emit sig_groupSettingsChanged(); // Removed
    }
}
