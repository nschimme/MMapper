// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Nils Schimmelmann <nschimme@gmail.com> (Jahara)

#include "graphicspage.h"

#include "../configuration/configuration.h"
#include "../global/utils.h"
#include "../opengl/OpenGLConfig.h"
#include "AdvancedGraphics.h"
#include "ui_graphicspage.h"

#include <memory>

#include <QString>
#include <QtGui>
#include <QtWidgets>

static void setIconColor(QPushButton *const button, const Color &color)
{
    QPixmap bgPix(16, 16);
    bgPix.fill(color.getQColor());
    button->setIcon(QIcon(bgPix));
}

GraphicsPage::GraphicsPage(QWidget *parent, Configuration &config)
    : QWidget(parent)
    , ui(new Ui::GraphicsPage)
    , m_config(config)
{
    ui->setupUi(this);

    m_advanced = std::make_unique<AdvancedGraphicsGroupBox>(deref(ui->groupBox_Advanced), m_config);

    connect(ui->bgChangeColor, &QAbstractButton::clicked, this, [this]() {
        if (changeColorClicked(m_config.canvas.backgroundColor)) {
            setIconColor(ui->bgChangeColor, m_config.canvas.backgroundColor);
        }
    });
    connect(ui->darkPushButton, &QAbstractButton::clicked, this, [this]() {
        if (changeColorClicked(m_config.canvas.roomDarkColor)) {
            setIconColor(ui->darkPushButton, m_config.canvas.roomDarkColor);
        }
    });
    connect(ui->darkLitPushButton, &QAbstractButton::clicked, this, [this]() {
        if (changeColorClicked(m_config.canvas.roomDarkLitColor)) {
            setIconColor(ui->darkLitPushButton, m_config.canvas.roomDarkLitColor);
        }
    });
    connect(ui->connectionNormalPushButton, &QAbstractButton::clicked, this, [this]() {
        if (changeColorClicked(m_config.canvas.connectionNormalColor)) {
            setIconColor(ui->connectionNormalPushButton, m_config.canvas.connectionNormalColor);
        }
    });
    connect(ui->antialiasingSamplesComboBox,
            &QComboBox::currentTextChanged,
            this,
            [this](const QString & /*text*/) {
                if (ui->antialiasingSamplesComboBox->isEnabled()) {
                    m_config.canvas.antialiasingSamples.set(
                        ui->antialiasingSamplesComboBox
                            ->itemData(ui->antialiasingSamplesComboBox->currentIndex())
                            .toInt());
                }
            });
    connect(ui->trilinearFilteringCheckBox, &QCheckBox::stateChanged, this, [this](int /*unused*/) {
        m_config.canvas.trilinearFiltering.set(ui->trilinearFilteringCheckBox->isChecked());
    });

    connect(ui->drawUnsavedChanges, &QCheckBox::stateChanged, this, [this](int /*unused*/) {
        m_config.canvas.showUnsavedChanges.set(ui->drawUnsavedChanges->isChecked());
    });
    connect(ui->drawNeedsUpdate,
            &QCheckBox::stateChanged,
            this,
            &GraphicsPage::slot_drawNeedsUpdateStateChanged);
    connect(ui->drawNotMappedExits,
            &QCheckBox::stateChanged,
            this,
            &GraphicsPage::slot_drawNotMappedExitsStateChanged);
    connect(ui->drawDoorNames,
            &QCheckBox::stateChanged,
            this,
            &GraphicsPage::slot_drawDoorNamesStateChanged);
    connect(ui->drawUpperLayersTextured,
            &QCheckBox::stateChanged,
            this,
            &GraphicsPage::slot_drawUpperLayersTexturedStateChanged);

    connect(ui->weatherAtmosphereSlider, &QSlider::valueChanged, this, [this](const int value) {
        m_config.canvas.weatherAtmosphereIntensity.set(value);
    });

    connect(ui->weatherPrecipitationSlider, &QSlider::valueChanged, this, [this](const int value) {
        m_config.canvas.weatherPrecipitationIntensity.set(value);
    });

    connect(ui->weatherTimeOfDaySlider, &QSlider::valueChanged, this, [this](const int value) {
        m_config.canvas.weatherTimeOfDayIntensity.set(value);
    });

}

GraphicsPage::~GraphicsPage()
{
    delete ui;
}

void GraphicsPage::slot_loadConfig()
{
    const auto &settings = m_config.canvas;
    setIconColor(ui->bgChangeColor, settings.backgroundColor);
    setIconColor(ui->darkPushButton, settings.roomDarkColor);
    setIconColor(ui->darkLitPushButton, settings.roomDarkLitColor);
    setIconColor(ui->connectionNormalPushButton, settings.connectionNormalColor);

    {
        ui->antialiasingSamplesComboBox->setEnabled(false);
        ui->antialiasingSamplesComboBox->clear();
        const int maxSamples = OpenGLConfig::getMaxSamples();
        for (int i = 0; i <= maxSamples; i *= 2) {
            ui->antialiasingSamplesComboBox->addItem(i != 0 ? QString("%1x").arg(i) : "Off", i);
            if (i == 0) {
                i = 1;
            }
        }
        const auto samples = std::min(settings.antialiasingSamples.get(), maxSamples);
        const int index = utils::clampNonNegative(
            ui->antialiasingSamplesComboBox->findData(QVariant(samples), Qt::UserRole));
        ui->antialiasingSamplesComboBox->setCurrentIndex(index);
        ui->antialiasingSamplesComboBox->setEnabled(true);
    }
    ui->trilinearFilteringCheckBox->setChecked(settings.trilinearFiltering.get());

    ui->drawUnsavedChanges->setChecked(settings.showUnsavedChanges.get());
    ui->drawNeedsUpdate->setChecked(settings.showMissingMapId.get());
    ui->drawNotMappedExits->setChecked(settings.showUnmappedExits.get());
    ui->drawDoorNames->setChecked(settings.drawDoorNames);

    ui->weatherAtmosphereSlider->setValue(settings.weatherAtmosphereIntensity.get());
    ui->weatherPrecipitationSlider->setValue(settings.weatherPrecipitationIntensity.get());
    ui->weatherTimeOfDaySlider->setValue(settings.weatherTimeOfDayIntensity.get());
}

bool GraphicsPage::changeColorClickedImpl(Color &color)
{
    const QColor origColor = color.getQColor();
    const QColor newColor = QColorDialog::getColor(origColor, this);
    if (newColor.isValid() && newColor != origColor) {
        color = Color(newColor);
        return true;
    }
    return false;
}

void GraphicsPage::slot_drawNeedsUpdateStateChanged(int /*unused*/)
{
    m_config.canvas.showMissingMapId.set(ui->drawNeedsUpdate->isChecked());
}

void GraphicsPage::slot_drawNotMappedExitsStateChanged(int /*unused*/)
{
    m_config.canvas.showUnmappedExits.set(ui->drawNotMappedExits->isChecked());
}

void GraphicsPage::slot_drawDoorNamesStateChanged(int /*unused*/)
{
    m_config.canvas.drawDoorNames.set(ui->drawDoorNames->isChecked());
}

void GraphicsPage::slot_drawUpperLayersTexturedStateChanged(int /*unused*/)
{
    m_config.canvas.drawUpperLayersTextured.set(ui->drawUpperLayersTextured->isChecked());
}
