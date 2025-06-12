// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2023 The MMapper Authors

#include "grouppage.h"
#include "../configuration/configuration.h" // Required for getConfig/setConfig

#include <QCheckBox>
#include <QPushButton>
#include <QLabel>
#include <QColorDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout> // Added for better layout of color chooser
#include <QGroupBox>
#include <QDebug> // For logging

GroupPage::GroupPage(QWidget *parent)
    : QWidget(parent)
    , m_selectedColor(getConfig().groupManager.color) // Initialize with current config
    , m_selectedNpcOverrideColor(getConfig().groupManager.npcOverrideColor) // Initialize NPC override color
{
    setupUi();
    slot_loadConfig(); // Load initial values
}

GroupPage::~GroupPage() = default;

void GroupPage::setupUi() {
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setAlignment(Qt::AlignTop);

    // NPC Filter Setting
    auto *filterGroupBox = new QGroupBox(tr("Filtering"), this);
    auto *filterLayout = new QVBoxLayout(filterGroupBox);

    m_filterNpcsCheckBox = new QCheckBox(tr("Filter out NPC characters from group display"), this);
    connect(m_filterNpcsCheckBox, &QCheckBox::stateChanged, this, &GroupPage::slot_filterNpcsChanged);
    filterLayout->addWidget(m_filterNpcsCheckBox);
    filterGroupBox->setLayout(filterLayout);
    mainLayout->addWidget(filterGroupBox);

    // Color Preference Setting
    auto *colorGroupBox = new QGroupBox(tr("Appearance"), this);
    auto *appearanceLayout = new QVBoxLayout(colorGroupBox); // Renamed from colorLayout for clarity

    // Character Color
    auto *charColorLayout = new QHBoxLayout();
    m_colorButton = new QPushButton(tr("Choose Your Character Color"), this);
    connect(m_colorButton, &QPushButton::clicked, this, &GroupPage::slot_chooseColor);
    charColorLayout->addWidget(m_colorButton);

    m_colorPreviewLabel = new QLabel(this);
    m_colorPreviewLabel->setFixedSize(20, 20);
    m_colorPreviewLabel->setAutoFillBackground(true);
    charColorLayout->addWidget(m_colorPreviewLabel);
    charColorLayout->addStretch();
    appearanceLayout->addLayout(charColorLayout);

    // NPC Color Override
    m_overrideNpcColorCheckBox = new QCheckBox(tr("Override NPC Colors"), this);
    connect(m_overrideNpcColorCheckBox, &QCheckBox::stateChanged, this, &GroupPage::sig_settingsChanged);
    appearanceLayout->addWidget(m_overrideNpcColorCheckBox);

    auto *npcColorLayout = new QHBoxLayout();
    m_npcOverrideColorButton = new QPushButton(tr("Choose NPC Override Color"), this);
    connect(m_npcOverrideColorButton, &QPushButton::clicked, this, &GroupPage::slot_chooseNpcOverrideColor);
    npcColorLayout->addWidget(m_npcOverrideColorButton);

    m_npcOverrideColorPreviewLabel = new QLabel(this);
    m_npcOverrideColorPreviewLabel->setFixedSize(20, 20);
    m_npcOverrideColorPreviewLabel->setAutoFillBackground(true);
    npcColorLayout->addWidget(m_npcOverrideColorPreviewLabel);
    npcColorLayout->addStretch();
    appearanceLayout->addLayout(npcColorLayout);

    // Initial state of NPC color button based on checkbox
    connect(m_overrideNpcColorCheckBox, &QCheckBox::toggled, m_npcOverrideColorButton, &QPushButton::setEnabled);
    connect(m_overrideNpcColorCheckBox, &QCheckBox::toggled, m_npcOverrideColorPreviewLabel, &QLabel::setVisible);


    // Sort NPCs to Bottom
    m_sortNpcsToBottomCheckBox = new QCheckBox(tr("Show NPCs at the bottom of the list"), this);
    connect(m_sortNpcsToBottomCheckBox, &QCheckBox::stateChanged, this, &GroupPage::sig_settingsChanged);
    appearanceLayout->addWidget(m_sortNpcsToBottomCheckBox);

    colorGroupBox->setLayout(appearanceLayout);
    mainLayout->addWidget(colorGroupBox);

    setLayout(mainLayout);
}

void GroupPage::slot_loadConfig() {
    const auto& groupManagerSettings = getConfig().groupManager;
    m_filterNpcsCheckBox->setChecked(groupManagerSettings.filterNPCs);
    m_selectedColor = groupManagerSettings.color;

    QPalette charPalette = m_colorPreviewLabel->palette();
    charPalette.setColor(QPalette::Window, m_selectedColor);
    m_colorPreviewLabel->setPalette(charPalette);

    // Load NPC override settings
    m_overrideNpcColorCheckBox->setChecked(groupManagerSettings.overrideNpcColor);
    m_selectedNpcOverrideColor = groupManagerSettings.npcOverrideColor;
    QPalette npcPalette = m_npcOverrideColorPreviewLabel->palette();
    npcPalette.setColor(QPalette::Window, m_selectedNpcOverrideColor);
    m_npcOverrideColorPreviewLabel->setPalette(npcPalette);

    // Set initial enabled state for NPC color chooser
    m_npcOverrideColorButton->setEnabled(groupManagerSettings.overrideNpcColor);
    m_npcOverrideColorPreviewLabel->setVisible(groupManagerSettings.overrideNpcColor);

    m_sortNpcsToBottomCheckBox->setChecked(groupManagerSettings.sortNpcsToBottom);

    qDebug() << "GroupPage: Config loaded. Filter NPCs:" << groupManagerSettings.filterNPCs
             << "Color:" << m_selectedColor.name()
             << "Override NPC Color:" << groupManagerSettings.overrideNpcColor
             << "NPC Override Color:" << m_selectedNpcOverrideColor.name()
             << "Sort NPCs to Bottom:" << groupManagerSettings.sortNpcsToBottom;
}

// This slot can be called by a general "Apply" or "OK" button in the ConfigDialog
// Or, individual changes could call it directly if auto-apply is desired.
void GroupPage::slot_saveConfig() {
    auto& groupManagerSettings = setConfig().groupManager;
    groupManagerSettings.filterNPCs = m_filterNpcsCheckBox->isChecked();
    groupManagerSettings.color = m_selectedColor;

    // Save NPC override settings
    groupManagerSettings.overrideNpcColor = m_overrideNpcColorCheckBox->isChecked();
    groupManagerSettings.npcOverrideColor = m_selectedNpcOverrideColor;
    groupManagerSettings.sortNpcsToBottom = m_sortNpcsToBottomCheckBox->isChecked();

    // Note: setConfig().write() should ideally be called by ConfigDialog when "OK" or "Apply" is clicked,
    // to save all page settings at once. For now, we assume this page might save individually if needed.
    // If ConfigDialog handles the global save, this direct call to write() might be redundant
    // or even undesirable if changes are meant to be transactional.
    // setConfig().write(); // Let ConfigDialog handle the actual QSettings write.

    qDebug() << "GroupPage: Config to be saved. Filter NPCs:" << groupManagerSettings.filterNPCs
             << "Color:" << m_selectedColor.name()
             << "Override NPC Color:" << groupManagerSettings.overrideNpcColor
             << "NPC Override Color:" << m_selectedNpcOverrideColor.name()
             << "Sort NPCs to Bottom:" << groupManagerSettings.sortNpcsToBottom;
    emit sig_settingsChanged(); // Notify that settings have changed
}

void GroupPage::slot_filterNpcsChanged(int state) {
    Q_UNUSED(state);
    // If settings are applied immediately:
    slot_saveConfig();
    // Otherwise, this just updates the internal state, and slot_saveConfig() is called by Apply/OK.
}

void GroupPage::slot_chooseColor() {
    const QColorDialog::ColorDialogOptions options = QFlag(QColorDialog::ShowAlphaChannel);
    const QColor color = QColorDialog::getColor(m_selectedColor, this, tr("Select Color"), options);

    if (color.isValid()) {
        m_selectedColor = color;
        QPalette palette = m_colorPreviewLabel->palette();
        palette.setColor(QPalette::Window, m_selectedColor);
        m_colorPreviewLabel->setPalette(palette);
        // If settings are applied immediately:
        // slot_saveConfig(); // Changed to emit signal
        emit sig_settingsChanged();
         // Otherwise, this just updates the internal state, and slot_saveConfig() is called by Apply/OK.
    }
}

void GroupPage::slot_chooseNpcOverrideColor() {
    const QColorDialog::ColorDialogOptions options = QFlag(QColorDialog::ShowAlphaChannel);
    const QColor color = QColorDialog::getColor(m_selectedNpcOverrideColor, this, tr("Select NPC Override Color"), options);

    if (color.isValid()) {
        m_selectedNpcOverrideColor = color;
        QPalette palette = m_npcOverrideColorPreviewLabel->palette();
        palette.setColor(QPalette::Window, m_selectedNpcOverrideColor);
        m_npcOverrideColorPreviewLabel->setPalette(palette);
        // If settings are applied immediately:
        // slot_saveConfig(); // Changed to emit signal
        emit sig_settingsChanged();
        // Otherwise, this just updates the internal state, and slot_saveConfig() is called by Apply/OK.
    }
}
