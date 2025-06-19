// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Nils Schimmelmann <nschimme@gmail.com> (Jahara)

#include "clientpage.h"

#include "../configuration/configuration.h"
// No need for NamedColors.h for IntegratedMudClientSettings specifically
#include "ui_clientpage.h"

#include <QFont>
#include <QFontComboBox> // Was forward declared in .h, ensure full include for connect if needed by type
#include <QFontInfo>
#include <QPalette>
#include <QSpinBox>
#include <QString>
#include <QtGui>
#include <QtWidgets>

ClientPage::ClientPage(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::ClientPage)
{
    ui->setupUi(this);

    // Initialize m_configLifetime and register callbacks
    m_configLifetime.disconnectAll(); // Ensure clean slate for connections
    Configuration &config = getConfig();

    config.integratedClient.registerChangeCallback(m_configLifetime, [this]() {
        // When settings change (possibly externally), refresh UI and emit signal.
        slot_loadConfig();
        emit sig_clientSettingsChanged();
    });

    // Note: The QFontComboBox is not used in the provided UI (ui->fontPushButton is a QPushButton)
    // If ui->fontComboBox existed, its connect would be:
    // connect(ui->fontComboBox, &QFontComboBox::currentFontChanged, this, [this](const QFont &font) {
    //    setConfig().integratedClient.setFont(font.toString());
    // });
    // Current font change is handled by slot_onChangeFont triggered by fontPushButton.

    connect(ui->fontPushButton, &QAbstractButton::pressed, this, &ClientPage::slot_onChangeFont);
    connect(ui->bgColorPushButton,
            &QAbstractButton::pressed,
            this,
            &ClientPage::slot_onChangeBackgroundColor);
    connect(ui->fgColorPushButton,
            &QAbstractButton::pressed,
            this,
            &ClientPage::slot_onChangeForegroundColor);

    connect(ui->columnsSpinBox,
            QOverload<int>::of(&QSpinBox::valueChanged),
            this,
            &ClientPage::slot_onChangeColumns);
    connect(ui->rowsSpinBox,
            QOverload<int>::of(&QSpinBox::valueChanged),
            this,
            &ClientPage::slot_onChangeRows);
    connect(ui->scrollbackSpinBox,
            QOverload<int>::of(&QSpinBox::valueChanged),
            this,
            &ClientPage::slot_onChangeLinesOfScrollback);
    connect(ui->previewSpinBox,
            QOverload<int>::of(&QSpinBox::valueChanged),
            this,
            [](const int value) { setConfig().integratedClient.setLinesOfPeekPreview(value); }); // Use setter

    connect(ui->inputHistorySpinBox,
            QOverload<int>::of(&QSpinBox::valueChanged),
            this,
            &ClientPage::slot_onChangeLinesOfInputHistory);
    connect(ui->tabDictionarySpinBox,
            QOverload<int>::of(&QSpinBox::valueChanged),
            this,
            &ClientPage::slot_onChangeTabCompletionDictionarySize);

    connect(ui->clearInputCheckBox, &QCheckBox::toggled, this, [](bool isChecked) { // Added 'this' for capture if needed by future changes
        setConfig().integratedClient.setClearInputOnEnter(isChecked); // Use setter
    });

    connect(ui->autoResizeTerminalCheckBox, &QCheckBox::toggled, this, [](bool isChecked) { // Added 'this'
        setConfig().integratedClient.setAutoResizeTerminal(isChecked); // Use setter
    });
}

ClientPage::~ClientPage()
{
    delete ui;
}

void ClientPage::slot_loadConfig()
{
    updateFontAndColors();

    const auto &settings = getConfig().integratedClient;
    ui->columnsSpinBox->setValue(settings.getColumns());
    ui->rowsSpinBox->setValue(settings.getRows());
    ui->scrollbackSpinBox->setValue(settings.getLinesOfScrollback());
    ui->previewSpinBox->setValue(settings.getLinesOfPeekPreview());
    ui->inputHistorySpinBox->setValue(settings.getLinesOfInputHistory());
    ui->tabDictionarySpinBox->setValue(settings.getTabCompletionDictionarySize());
    ui->clearInputCheckBox->setChecked(settings.getClearInputOnEnter());
    ui->autoResizeTerminalCheckBox->setChecked(settings.getAutoResizeTerminal());
}

void ClientPage::updateFontAndColors()
{
    const auto &settings = getConfig().integratedClient;
    QFont font;
    font.fromString(settings.getFont()); // Use getter
    ui->exampleLabel->setFont(font);

    QFontInfo fi(font);
    ui->fontPushButton->setText(
        QString("%1 %2, %3").arg(fi.family()).arg(fi.styleName()).arg(fi.pointSize()));

    QPixmap fgPix(16, 16);
    fgPix.fill(settings.getForegroundColor()); // Use getter
    ui->fgColorPushButton->setIcon(QIcon(fgPix));

    QPixmap bgPix(16, 16);
    bgPix.fill(settings.getBackgroundColor()); // Use getter
    ui->bgColorPushButton->setIcon(QIcon(bgPix));

    QPalette palette = ui->exampleLabel->palette();
    ui->exampleLabel->setAutoFillBackground(true);
    palette.setColor(QPalette::WindowText, settings.getForegroundColor()); // Use getter
    palette.setColor(QPalette::Window, settings.getBackgroundColor()); // Use getter
    ui->exampleLabel->setPalette(palette);
    ui->exampleLabel->setBackgroundRole(QPalette::Window);
}

void ClientPage::slot_onChangeFont()
{
    // auto &fontDescription = setConfig().integratedClient.font; // Old way
    QFont oldFont;
    oldFont.fromString(getConfig().integratedClient.getFont()); // Use getter

    bool ok = false;
    const QFont newFont = QFontDialog::getFont(&ok,
                                               oldFont,
                                               this,
                                               "Select Font",
                                               QFontDialog::MonospacedFonts);
    if (ok) {
        // fontDescription = newFont.toString(); // Old way
        setConfig().integratedClient.setFont(newFont.toString()); // Use setter
        updateFontAndColors(); // Keep UI update for immediate feedback
    }
}

void ClientPage::slot_onChangeBackgroundColor()
{
    // auto &backgroundColor = setConfig().integratedClient.backgroundColor; // Old way
    const QColor oldColor = getConfig().integratedClient.getBackgroundColor(); // Use getter
    const QColor newColor = QColorDialog::getColor(oldColor, this);
    if (newColor.isValid() && newColor != oldColor) {
        // backgroundColor = newColor; // Old way
        setConfig().integratedClient.setBackgroundColor(newColor); // Use setter
        updateFontAndColors(); // Keep UI update
    }
}

void ClientPage::slot_onChangeForegroundColor()
{
    // auto &foregroundColor = setConfig().integratedClient.foregroundColor; // Old way
    const QColor oldColor = getConfig().integratedClient.getForegroundColor(); // Use getter
    const QColor newColor = QColorDialog::getColor(oldColor, this);
    if (newColor.isValid() && newColor != oldColor) {
        // foregroundColor = newColor; // Old way
        setConfig().integratedClient.setForegroundColor(newColor); // Use setter
        updateFontAndColors(); // Keep UI update
    }
}

void ClientPage::slot_onChangeColumns(const int value)
{
    setConfig().integratedClient.setColumns(value); // Use setter
}

void ClientPage::slot_onChangeRows(const int value)
{
    setConfig().integratedClient.setRows(value); // Use setter
}

void ClientPage::slot_onChangeLinesOfScrollback(const int value)
{
    setConfig().integratedClient.setLinesOfScrollback(value); // Use setter
}

void ClientPage::slot_onChangeLinesOfInputHistory(const int value)
{
    setConfig().integratedClient.setLinesOfInputHistory(value); // Use setter
}

void ClientPage::slot_onChangeTabCompletionDictionarySize(const int value)
{
    setConfig().integratedClient.setTabCompletionDictionarySize(value); // Use setter
}
