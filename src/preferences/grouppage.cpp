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
#include <QGroupBox>
#include <QDebug> // For logging

GroupPage::GroupPage(QWidget *parent)
    : QWidget(parent)
    , m_selectedColor(getConfig().groupManager.color) // Initialize with current config
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
    auto *colorLayout = new QVBoxLayout(colorGroupBox);
    auto *colorSelectionLayout = new QHBoxLayout(); // For button and preview

    m_colorButton = new QPushButton(tr("Choose Your Character Color"), this);
    connect(m_colorButton, &QPushButton::clicked, this, &GroupPage::slot_chooseColor);
    colorSelectionLayout->addWidget(m_colorButton);

    m_colorPreviewLabel = new QLabel(this);
    m_colorPreviewLabel->setFixedSize(20, 20); // Small square to show color
    m_colorPreviewLabel->setAutoFillBackground(true);
    colorSelectionLayout->addWidget(m_colorPreviewLabel);
    colorSelectionLayout->addStretch(); // Push button and label to the left

    colorLayout->addLayout(colorSelectionLayout);
    colorGroupBox->setLayout(colorLayout);
    mainLayout->addWidget(colorGroupBox);

    setLayout(mainLayout);
}

void GroupPage::slot_loadConfig() {
    const auto& groupManagerSettings = getConfig().groupManager;
    m_filterNpcsCheckBox->setChecked(groupManagerSettings.filterNPCs);
    m_selectedColor = groupManagerSettings.color;

    QPalette palette = m_colorPreviewLabel->palette();
    palette.setColor(QPalette::Window, m_selectedColor);
    m_colorPreviewLabel->setPalette(palette);

    qDebug() << "GroupPage: Config loaded. Filter NPCs:" << groupManagerSettings.filterNPCs << "Color:" << m_selectedColor.name();
}

// This slot can be called by a general "Apply" or "OK" button in the ConfigDialog
// Or, individual changes could call it directly if auto-apply is desired.
void GroupPage::slot_saveConfig() {
    auto& groupManagerSettings = setConfig().groupManager;
    groupManagerSettings.filterNPCs = m_filterNpcsCheckBox->isChecked();
    groupManagerSettings.color = m_selectedColor;

    // Note: setConfig().write() should ideally be called by ConfigDialog when "OK" or "Apply" is clicked,
    // to save all page settings at once. For now, we assume this page might save individually if needed.
    // If ConfigDialog handles the global save, this direct call to write() might be redundant
    // or even undesirable if changes are meant to be transactional.
    // setConfig().write(); // Let ConfigDialog handle the actual QSettings write.

    qDebug() << "GroupPage: Config to be saved. Filter NPCs:" << groupManagerSettings.filterNPCs << "Color:" << m_selectedColor.name();
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
        slot_saveConfig();
         // Otherwise, this just updates the internal state, and slot_saveConfig() is called by Apply/OK.
    }
}
