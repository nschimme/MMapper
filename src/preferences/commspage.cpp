// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#include "commspage.h"

#include <QColorDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QPushButton>
#include <QVBoxLayout>

#include "../configuration/configuration.h"

CommsPage::CommsPage(QWidget *parent)
    : QWidget(parent)
{
    setupUI();
    connectSignals();
    slot_loadConfig();
}

CommsPage::~CommsPage() = default;

void CommsPage::createColorButton(QFormLayout *layout,
                                  const QString &label,
                                  NamedConfig<QColor> &config)
{
    auto *button = new QPushButton(tr("Choose Color..."), this);
    button->setMinimumWidth(120);
    layout->addRow(label, button);
    m_colorSettings.append({button, &config, label});
}

void CommsPage::createCheckbox(QFormLayout *layout, NamedConfig<bool> &config)
{
    auto *checkbox = new QCheckBox(QString::fromStdString(config.getName()), this);
    layout->addRow(checkbox);
    m_checkboxSettings.append(CheckboxSetting{checkbox, &config});
}

void CommsPage::setupUI()
{
    auto *mainLayout = new QVBoxLayout(this);
    setLayout(mainLayout);

    auto &comms = setConfig().comms;

    // Talker Colors Group
    auto *talkerColorsGroup = new QGroupBox(tr("Talker Colors"), this);
    auto *talkerColorsLayout = new QFormLayout(talkerColorsGroup);
    mainLayout->addWidget(talkerColorsGroup);

    createColorButton(talkerColorsLayout, tr("You (sent messages):"), comms.talkerYouColor);
    createColorButton(talkerColorsLayout, tr("Player:"), comms.talkerPlayerColor);
    createColorButton(talkerColorsLayout, tr("NPC:"), comms.talkerNpcColor);
    createColorButton(talkerColorsLayout, tr("Ally:"), comms.talkerAllyColor);
    createColorButton(talkerColorsLayout, tr("Neutral:"), comms.talkerNeutralColor);
    createColorButton(talkerColorsLayout, tr("Enemy:"), comms.talkerEnemyColor);

    // Communication Colors Group
    auto *colorsGroup = new QGroupBox(tr("Communication Colors"), this);
    auto *colorsLayout = new QFormLayout(colorsGroup);
    mainLayout->addWidget(colorsGroup);

    createColorButton(colorsLayout, tr("Tell:"), comms.tellColor);
    createColorButton(colorsLayout, tr("Whisper:"), comms.whisperColor);
    createColorButton(colorsLayout, tr("Group:"), comms.groupColor);
    createColorButton(colorsLayout, tr("Question:"), comms.askColor);
    createColorButton(colorsLayout, tr("Say:"), comms.sayColor);
    createColorButton(colorsLayout, tr("Emote:"), comms.emoteColor);
    createColorButton(colorsLayout, tr("Social:"), comms.socialColor);
    createColorButton(colorsLayout, tr("Yell:"), comms.yellColor);
    createColorButton(colorsLayout, tr("Tale:"), comms.narrateColor);
    createColorButton(colorsLayout, tr("Song:"), comms.singColor);
    createColorButton(colorsLayout, tr("Prayer:"), comms.prayColor);
    createColorButton(colorsLayout, tr("Shout:"), comms.shoutColor);
    createColorButton(colorsLayout, tr("Background:"), comms.backgroundColor);

    // Font Styling Group
    auto *fontGroup = new QGroupBox(tr("Font Styling and Display"), this);
    auto *fontLayout = new QFormLayout(fontGroup);
    mainLayout->addWidget(fontGroup);

    createCheckbox(fontLayout, comms.yellAllCaps);
    createCheckbox(fontLayout, comms.whisperItalic);
    createCheckbox(fontLayout, comms.emoteItalic);
    createCheckbox(fontLayout, comms.showTimestamps);

    mainLayout->addStretch();
}

void CommsPage::connectSignals()
{
    for (const auto &setting : qAsConst(m_colorSettings)) {
        connect(setting.button, &QPushButton::clicked, this, &CommsPage::slot_onColorClicked);
    }
    for (const auto &setting : qAsConst(m_checkboxSettings)) {
        connect(setting.checkbox,
                &QCheckBox::toggled,
                this,
                &CommsPage::slot_onCheckboxToggled);
    }
}

void CommsPage::slot_loadConfig()
{
    for (const auto &setting : qAsConst(m_colorSettings)) {
        updateColorButton(setting.button, setting.config->get());
    }
    for (const auto &setting : qAsConst(m_checkboxSettings)) {
        setting.checkbox->setChecked(setting.config->get());
    }
}

void CommsPage::updateColorButton(QPushButton *button, const QColor &color)
{
    if (!button) {
        return;
    }

    // Set button background to the color and text color to black or white
    const QString style = QString("background-color: %1; color: %2;")
                              .arg(color.name())
                              .arg(color.lightnessF() > 0.5f ? "black" : "white");
    button->setStyleSheet(style);
}

void CommsPage::slot_onColorClicked()
{
    auto *button = qobject_cast<QPushButton *>(sender());
    if (!button) {
        return;
    }

    // Find the setting associated with this button
    for (auto &setting : m_colorSettings) {
        if (setting.button == button) {
            const QColor currentColor = setting.config->get();
            const QString dialogTitle = tr("Choose %1 Color").arg(setting.label);
            const QColor newColor = QColorDialog::getColor(currentColor, this, dialogTitle);

            if (newColor.isValid() && newColor != currentColor) {
                setting.config->set(newColor);
                updateColorButton(button, newColor);
                emit sig_commsSettingsChanged();
            }
            return; // Found and handled
        }
    }
}

void CommsPage::slot_onCheckboxToggled(const bool checked)
{
    auto *checkbox = qobject_cast<QCheckBox *>(sender());
    if (!checkbox) {
        return;
    }

    // Find the setting associated with this checkbox
    for (auto &setting : m_checkboxSettings) {
        if (setting.checkbox == checkbox) {
            setting.config->set(checked);
            emit sig_commsSettingsChanged();
            return; // Found and handled
        }
    }
}