// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Nils Schimmelmann <nschimme@gmail.com> (Jahara)

#include "graphicspage.h"

#include "../configuration/configuration.h"
#include "../global/ConfigConsts.h"
#include "../global/utils.h"
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

    {
        QGridLayout *layout = ui->gridLayout_3;
        int row = 0;
#define ADD_COLOR_PICKER(_id, _name, _uiName) \
    if (_uiName) { \
        auto *label = new QLabel(tr(_uiName)); \
        auto *button = new QPushButton(tr("Select")); \
        button->setObjectName(QString::fromUtf8(_name)); \
        layout->addWidget(label, row, 0); \
        layout->addWidget(button, row, 1); \
        label->setBuddy(button); \
        connect(button, &QAbstractButton::clicked, this, [this, button]() { \
            changeColorClicked(setConfig().canvas._id, button); \
            graphicsSettingsChanged(); \
        }); \
        row++; \
    }
        XFOREACH_CANVAS_NAMED_COLOR_OPTIONS(ADD_COLOR_PICKER)
#undef ADD_COLOR_PICKER

        layout->addItem(new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum), \
                        0, \
                        2, \
                        row, \
                        1);
        layout->setColumnStretch(2, 1);
    }

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
        graphicsSettingsChanged();
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

    connect(ui->resourceLineEdit, &QLineEdit::textChanged, this, [](const QString &text) {
        setConfig().canvas.resourcesDirectory = text;
    });
    connect(ui->resourcePushButton, &QAbstractButton::clicked, this, [this](bool /*unused*/) {
        const auto &resourceDir = getConfig().canvas.resourcesDirectory;
        QString directory = QFileDialog::getExistingDirectory(ui->resourcePushButton,
                                                              "Choose resource location ...",
                                                              resourceDir);
        if (!directory.isEmpty()) {
            ui->resourceLineEdit->setText(directory);
            setConfig().canvas.resourcesDirectory = directory;
        }
    });

    connect(m_advanced.get(),
            &AdvancedGraphicsGroupBox::sig_graphicsSettingsChanged,
            this,
            &GraphicsPage::slot_graphicsSettingsChanged);

    if constexpr (CURRENT_PLATFORM == PlatformEnum::Wasm) {
        ui->resourceLineEdit->setDisabled(true);
        ui->resourcePushButton->setDisabled(true);
    }
}

GraphicsPage::~GraphicsPage()
{
    delete ui;
}

void GraphicsPage::slot_loadConfig()
{
    const auto &canvas = getConfig().canvas;

#define SET_BUTTON_ICON(_id, _name, _uiName) \
    if (_uiName) { \
        auto *button = ui->groupBox_Colour->findChild<QPushButton *>(QString::fromUtf8(_name)); \
        if (button) { \
            setIconColor(button, canvas._id); \
        } \
    }
    XFOREACH_CANVAS_NAMED_COLOR_OPTIONS(SET_BUTTON_ICON)
#undef SET_BUTTON_ICON

    const QString antiAliasingSamples = QString::number(canvas.antialiasingSamples);
    const int index = utils::clampNonNegative(
        ui->antialiasingSamplesComboBox->findText(antiAliasingSamples));
    ui->antialiasingSamplesComboBox->setCurrentIndex(index);
    ui->trilinearFilteringCheckBox->setChecked(canvas.trilinearFiltering);

    ui->drawUnsavedChanges->setChecked(canvas.showUnsavedChanges.get());
    ui->drawNeedsUpdate->setChecked(canvas.showMissingMapId.get());
    ui->drawNotMappedExits->setChecked(canvas.showUnmappedExits.get());
    ui->drawDoorNames->setChecked(canvas.drawDoorNames);

    ui->resourceLineEdit->setText(canvas.resourcesDirectory);
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
    setConfig().canvas.antialiasingSamples = ui->antialiasingSamplesComboBox->currentText().toInt();
    graphicsSettingsChanged();
}

void GraphicsPage::slot_trilinearFilteringStateChanged(int /*unused*/)
{
    setConfig().canvas.trilinearFiltering = ui->trilinearFilteringCheckBox->isChecked();
    graphicsSettingsChanged();
}

void GraphicsPage::slot_drawNeedsUpdateStateChanged(int /*unused*/)
{
    setConfig().canvas.showMissingMapId.set(ui->drawNeedsUpdate->isChecked());
    graphicsSettingsChanged();
}

void GraphicsPage::slot_drawNotMappedExitsStateChanged(int /*unused*/)
{
    setConfig().canvas.showUnmappedExits.set(ui->drawNotMappedExits->isChecked());
    graphicsSettingsChanged();
}

void GraphicsPage::slot_drawDoorNamesStateChanged(int /*unused*/)
{
    setConfig().canvas.drawDoorNames = ui->drawDoorNames->isChecked();
    graphicsSettingsChanged();
}

void GraphicsPage::slot_drawUpperLayersTexturedStateChanged(int /*unused*/)
{
    setConfig().canvas.drawUpperLayersTextured = ui->drawUpperLayersTextured->isChecked();
    graphicsSettingsChanged();
}
