// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Nils Schimmelmann <nschimme@gmail.com> (Jahara)

#include "graphicspage.h"

#include "../configuration/configuration.h"
#include "../global/utils.h"
#include "../global/NamedColors.h" // Added
#include "AdvancedGraphics.h"
#include "ui_graphicspage.h"

#include <memory>

#include <QString>
#include <QtGui>
#include <QtWidgets>

static void setIconColor(QPushButton *const button, const XNamedColor &namedColor)
{
    QPixmap bgPix(16, 16);
    bgPix.fill(namedColor.getColor().getQColor());
    button->setIcon(QIcon(bgPix));
}

GraphicsPage::GraphicsPage(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::GraphicsPage)
{
    ui->setupUi(this);

    m_advanced = std::make_unique<AdvancedGraphicsGroupBox>(deref(ui->groupBox_Advanced));

    // Initialize m_configLifetime
    m_configLifetime.disconnectAll();

    Configuration &config = getConfig();

    // Connect to CanvasSettings general changes
    config.canvas.registerChangeCallback(m_configLifetime, [this]() {
        this->slot_loadConfig(); // Ensure UI refresh
        this->graphicsSettingsChanged();
    });

    // Connect to global XNamedColor changes
    XNamedColor::registerGlobalChangeCallback(m_configLifetime, [this]() {
        this->slot_loadConfig();
        this->graphicsSettingsChanged();
    });

    // Connect to specific NamedConfig<T> properties in config.canvas
    config.canvas.showUnsavedChanges.registerChangeCallback(m_configLifetime, [this]() {
        this->slot_loadConfig(); // Ensure UI refresh
        this->graphicsSettingsChanged();
    });
    config.canvas.showMissingMapId.registerChangeCallback(m_configLifetime, [this]() {
        this->slot_loadConfig(); // Ensure UI refresh
        this->graphicsSettingsChanged();
    });
    config.canvas.showUnmappedExits.registerChangeCallback(m_configLifetime, [this]() {
        this->slot_loadConfig(); // Ensure UI refresh
        this->graphicsSettingsChanged();
    });

    connect(ui->bgChangeColor, &QAbstractButton::clicked, this, [this]() {
        changeColorClicked(setConfig().canvas.backgroundColor, ui->bgChangeColor);
        // graphicsSettingsChanged(); // Removed: Now handled by XNamedColor's global ChangeMonitor
    });
    connect(ui->darkPushButton, &QAbstractButton::clicked, this, [this]() {
        changeColorClicked(setConfig().canvas.roomDarkColor, ui->darkPushButton);
        // graphicsSettingsChanged(); // Removed: Now handled by XNamedColor's global ChangeMonitor
    });
    connect(ui->darkLitPushButton, &QAbstractButton::clicked, this, [this]() {
        changeColorClicked(setConfig().canvas.roomDarkLitColor, ui->darkLitPushButton);
        // graphicsSettingsChanged(); // Removed: Now handled by XNamedColor's global ChangeMonitor
    });
    connect(ui->connectionNormalPushButton, &QAbstractButton::clicked, this, [this]() {
        changeColorClicked(setConfig().canvas.connectionNormalColor, ui->connectionNormalPushButton);
        // graphicsSettingsChanged(); // Removed: Now handled by XNamedColor's global ChangeMonitor
    });
    connect(ui->antialiasingSamplesComboBox,
            &QComboBox::currentTextChanged,
            this,
            &GraphicsPage::slot_antialiasingSamplesTextChanged);
    connect(ui->trilinearFilteringCheckBox,
            &QCheckBox::stateChanged,
            this,
            &GraphicsPage::slot_trilinearFilteringStateChanged);

    connect(ui->drawUnsavedChanges, &QCheckBox::stateChanged, this, [this](int /*unused*/) {
        setConfig().canvas.showUnsavedChanges.set(ui->drawUnsavedChanges->isChecked());
        // graphicsSettingsChanged(); // Removed
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

    // For resourcesDirectory, direct lambda modification
    connect(ui->resourceLineEdit, &QLineEdit::textChanged, this, [this](const QString &text) {
        setConfig().canvas.setResourcesDirectory(text);
        // graphicsSettingsChanged(); // Removed: To be handled by ChangeMonitor
    });
    connect(ui->resourcePushButton, &QAbstractButton::clicked, this, [this](bool /*unused*/) {
        const auto &resourceDir = getConfig().canvas.getResourcesDirectory(); // Use getter
        QString directory = QFileDialog::getExistingDirectory(ui->resourcePushButton,
                                                              "Choose resource location ...",
                                                              resourceDir);
        if (!directory.isEmpty()) {
            ui->resourceLineEdit->setText(directory);
            // setConfig().canvas.resourcesDirectory = directory; // Old
            // setConfig().canvas.setResourcesDirectory(directory); // New, already handled by textChanged signal, but good for consistency if button directly set it
        }
    });

    connect(m_advanced.get(),
            &AdvancedGraphicsGroupBox::sig_graphicsSettingsChanged,
            this,
            &GraphicsPage::slot_graphicsSettingsChanged);
}

GraphicsPage::~GraphicsPage()
{
    delete ui;
}

void GraphicsPage::slot_loadConfig()
{
    const auto &settings = getConfig().canvas;
    setIconColor(ui->bgChangeColor, settings.backgroundColor);
    setIconColor(ui->darkPushButton, settings.roomDarkColor);
    setIconColor(ui->darkLitPushButton, settings.roomDarkLitColor);
    setIconColor(ui->connectionNormalPushButton, settings.connectionNormalColor);

    const QString antiAliasingSamples = QString::number(settings.getAntialiasingSamples()); // Use getter
    const int index = utils::clampNonNegative(
        ui->antialiasingSamplesComboBox->findText(antiAliasingSamples));
    ui->antialiasingSamplesComboBox->setCurrentIndex(index);
    ui->trilinearFilteringCheckBox->setChecked(settings.getTrilinearFiltering()); // Use getter

    ui->drawUnsavedChanges->setChecked(settings.showUnsavedChanges.get());
    ui->drawNeedsUpdate->setChecked(settings.showMissingMapId.get());
    ui->drawNotMappedExits->setChecked(settings.showUnmappedExits.get());
    ui->drawDoorNames->setChecked(settings.getDrawDoorNames()); // Use getter
    // softwareOpenGL is not on this page.
    ui->resourceLineEdit->setText(settings.getResourcesDirectory()); // Use getter
}

void GraphicsPage::changeColorClicked(XNamedColor &namedColor, QPushButton *const pushButton)
{
    const QColor origColor = namedColor.getColor().getQColor();
    const QColor newColor = QColorDialog::getColor(origColor, this);
    if (newColor.isValid() && newColor != origColor) {
        namedColor = Color(newColor);
        setIconColor(pushButton, namedColor);
    }
}

void GraphicsPage::slot_antialiasingSamplesTextChanged(const QString & /*unused*/)
{
    setConfig().canvas.setAntialiasingSamples(ui->antialiasingSamplesComboBox->currentText().toInt()); // Use setter
    // graphicsSettingsChanged(); // Removed: To be handled by ChangeMonitor
}

void GraphicsPage::slot_trilinearFilteringStateChanged(int /*unused*/)
{
    setConfig().canvas.setTrilinearFiltering(ui->trilinearFilteringCheckBox->isChecked()); // Use setter
    // graphicsSettingsChanged(); // Removed: To be handled by ChangeMonitor
}

void GraphicsPage::slot_drawNeedsUpdateStateChanged(int /*unused*/)
{
    setConfig().canvas.showMissingMapId.set(ui->drawNeedsUpdate->isChecked());
    // graphicsSettingsChanged(); // Removed
}

void GraphicsPage::slot_drawNotMappedExitsStateChanged(int /*unused*/)
{
    setConfig().canvas.showUnmappedExits.set(ui->drawNotMappedExits->isChecked());
    // graphicsSettingsChanged(); // Removed
}

void GraphicsPage::slot_drawDoorNamesStateChanged(int /*unused*/)
{
    setConfig().canvas.setDrawDoorNames(ui->drawDoorNames->isChecked()); // Use setter
    // graphicsSettingsChanged(); // Removed: To be handled by ChangeMonitor
}

void GraphicsPage::slot_drawUpperLayersTexturedStateChanged(int /*unused*/)
{
    setConfig().canvas.setDrawUpperLayersTextured(ui->drawUpperLayersTextured->isChecked()); // Use setter
    // graphicsSettingsChanged(); // Removed: To be handled by ChangeMonitor
}
