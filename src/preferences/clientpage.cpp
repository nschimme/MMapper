// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Nils Schimmelmann <nschimme@gmail.com> (Jahara)

#include "clientpage.h"

#include "../configuration/configuration.h"
#include "../global/SignalBlocker.h"
#include "../global/macros.h"
#include "ui_clientpage.h"

#include <QFont>
#include <QFontInfo>
#include <QPalette>
#include <QSpinBox>
#include <QString>
#include <QValidator>
#include <QtGui>
#include <QtWidgets>

class NODISCARD CustomSeparatorValidator final : public QValidator
{
public:
    explicit CustomSeparatorValidator(QObject *parent);
    ~CustomSeparatorValidator() final;

    void fixup(QString &input) const override
    {
        mmqt::toLatin1InPlace(input); // transliterates non-latin1 codepoints
    }

    QValidator::State validate(QString &input, int & /* pos */) const override
    {
        if (input.length() != 1) {
            return QValidator::State::Intermediate;
        }

        const auto c = input.front();
        const bool valid = c != char_consts::C_BACKSLASH && c.isPrint() && !c.isSpace();
        return valid ? QValidator::State::Acceptable : QValidator::State::Invalid;
    }
};

CustomSeparatorValidator::CustomSeparatorValidator(QObject *const parent)
    : QValidator(parent)
{}

CustomSeparatorValidator::~CustomSeparatorValidator() = default;

ClientPage::ClientPage(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::ClientPage)
{
    ui->setupUi(this);

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
            [](const int value) { setConfig().integratedClient.linesOfPeekPreview.set(value); });

    connect(ui->inputHistorySpinBox,
            QOverload<int>::of(&QSpinBox::valueChanged),
            this,
            &ClientPage::slot_onChangeLinesOfInputHistory);
    connect(ui->tabDictionarySpinBox,
            QOverload<int>::of(&QSpinBox::valueChanged),
            this,
            &ClientPage::slot_onChangeTabCompletionDictionarySize);

    connect(ui->clearInputCheckBox, &QCheckBox::toggled, [](bool isChecked) {
        /* NOTE: This directly modifies the global setting. */
        setConfig().integratedClient.clearInputOnEnter.set(isChecked);
    });

    connect(ui->autoResizeTerminalCheckBox, &QCheckBox::toggled, [](bool isChecked) {
        /* NOTE: This directly modifies the global setting. */
        setConfig().integratedClient.autoResizeTerminal.set(isChecked);
    });

    connect(ui->audibleBellCheckBox, &QCheckBox::toggled, [](bool isChecked) {
        setConfig().integratedClient.audibleBell.set(isChecked);
    });

    connect(ui->visualBellCheckBox, &QCheckBox::toggled, [](bool isChecked) {
        setConfig().integratedClient.visualBell.set(isChecked);
    });

    connect(ui->commandSeparatorCheckBox, &QCheckBox::toggled, this, [this](bool isChecked) {
        setConfig().integratedClient.useCommandSeparator.set(isChecked);
        ui->commandSeparatorLineEdit->setEnabled(isChecked);
    });

    connect(ui->commandSeparatorLineEdit, &QLineEdit::textChanged, this, [](const QString &text) {
        if (text.length() == 1) {
            setConfig().integratedClient.commandSeparator.set(text);
        }
    });

    ui->commandSeparatorLineEdit->setValidator(new CustomSeparatorValidator(this));

    auto &client = setConfig().integratedClient;
    client.font.registerChangeCallback(m_lifetime, [this]() { slot_loadConfig(); });
    client.foregroundColor.registerChangeCallback(m_lifetime, [this]() { slot_loadConfig(); });
    client.backgroundColor.registerChangeCallback(m_lifetime, [this]() { slot_loadConfig(); });
    client.columns.registerChangeCallback(m_lifetime, [this]() { slot_loadConfig(); });
    client.rows.registerChangeCallback(m_lifetime, [this]() { slot_loadConfig(); });
    client.linesOfScrollback.registerChangeCallback(m_lifetime, [this]() { slot_loadConfig(); });
    client.linesOfInputHistory.registerChangeCallback(m_lifetime, [this]() { slot_loadConfig(); });
    client.tabCompletionDictionarySize.registerChangeCallback(m_lifetime, [this]() { slot_loadConfig(); });
    client.clearInputOnEnter.registerChangeCallback(m_lifetime, [this]() { slot_loadConfig(); });
    client.autoResizeTerminal.registerChangeCallback(m_lifetime, [this]() { slot_loadConfig(); });
    client.linesOfPeekPreview.registerChangeCallback(m_lifetime, [this]() { slot_loadConfig(); });
    client.audibleBell.registerChangeCallback(m_lifetime, [this]() { slot_loadConfig(); });
    client.visualBell.registerChangeCallback(m_lifetime, [this]() { slot_loadConfig(); });
    client.useCommandSeparator.registerChangeCallback(m_lifetime, [this]() { slot_loadConfig(); });
    client.commandSeparator.registerChangeCallback(m_lifetime, [this]() { slot_loadConfig(); });
}

ClientPage::~ClientPage()
{
    delete ui;
}

void ClientPage::slot_loadConfig()
{
    SignalBlocker b1(*ui->columnsSpinBox);
    SignalBlocker b2(*ui->rowsSpinBox);
    SignalBlocker b3(*ui->scrollbackSpinBox);
    SignalBlocker b4(*ui->previewSpinBox);
    SignalBlocker b5(*ui->inputHistorySpinBox);
    SignalBlocker b6(*ui->tabDictionarySpinBox);
    SignalBlocker b7(*ui->clearInputCheckBox);
    SignalBlocker b8(*ui->autoResizeTerminalCheckBox);
    SignalBlocker b9(*ui->audibleBellCheckBox);
    SignalBlocker b10(*ui->visualBellCheckBox);
    SignalBlocker b11(*ui->commandSeparatorCheckBox);
    SignalBlocker b12(*ui->commandSeparatorLineEdit);

    updateFontAndColors();

    const auto &settings = getConfig().integratedClient;
    ui->columnsSpinBox->setValue(settings.columns.get());
    ui->rowsSpinBox->setValue(settings.rows.get());
    ui->scrollbackSpinBox->setValue(settings.linesOfScrollback.get());
    ui->previewSpinBox->setValue(settings.linesOfPeekPreview.get());
    ui->inputHistorySpinBox->setValue(settings.linesOfInputHistory.get());
    ui->tabDictionarySpinBox->setValue(settings.tabCompletionDictionarySize.get());
    ui->clearInputCheckBox->setChecked(settings.clearInputOnEnter.get());
    ui->autoResizeTerminalCheckBox->setChecked(settings.autoResizeTerminal.get());
    ui->audibleBellCheckBox->setChecked(settings.audibleBell.get());
    ui->visualBellCheckBox->setChecked(settings.visualBell.get());
    ui->commandSeparatorCheckBox->setChecked(settings.useCommandSeparator.get());
    ui->commandSeparatorLineEdit->setText(settings.commandSeparator.get());
    ui->commandSeparatorLineEdit->setEnabled(settings.useCommandSeparator.get());
}

void ClientPage::updateFontAndColors()
{
    const auto &settings = getConfig().integratedClient;
    QFont font;
    font.fromString(settings.font.get());
    ui->exampleLabel->setFont(font);

    QFontInfo fi(font);
    ui->fontPushButton->setText(
        QString("%1 %2, %3").arg(fi.family()).arg(fi.styleName()).arg(fi.pointSize()));

    QPixmap fgPix(16, 16);
    fgPix.fill(settings.foregroundColor.get());
    ui->fgColorPushButton->setIcon(QIcon(fgPix));

    QPixmap bgPix(16, 16);
    bgPix.fill(settings.backgroundColor.get());
    ui->bgColorPushButton->setIcon(QIcon(bgPix));

    QPalette palette = ui->exampleLabel->palette();
    ui->exampleLabel->setAutoFillBackground(true);
    palette.setColor(QPalette::WindowText, settings.foregroundColor.get());
    palette.setColor(QPalette::Window, settings.backgroundColor.get());
    ui->exampleLabel->setPalette(palette);
    ui->exampleLabel->setBackgroundRole(QPalette::Window);
}

void ClientPage::slot_onChangeFont()
{
    auto &fontDescription = setConfig().integratedClient.font;
    QFont oldFont;
    oldFont.fromString(fontDescription.get());

    bool ok = false;
    const QFont newFont = QFontDialog::getFont(&ok,
                                               oldFont,
                                               this,
                                               "Select Font",
                                               QFontDialog::MonospacedFonts);
    if (ok) {
        fontDescription.set(newFont.toString());
        updateFontAndColors();
    }
}

void ClientPage::slot_onChangeBackgroundColor()
{
    auto &backgroundColor = setConfig().integratedClient.backgroundColor;
    const QColor newColor = QColorDialog::getColor(backgroundColor.get(), this);
    if (newColor.isValid() && newColor != backgroundColor.get()) {
        backgroundColor.set(newColor);
        updateFontAndColors();
    }
}

void ClientPage::slot_onChangeForegroundColor()
{
    auto &foregroundColor = setConfig().integratedClient.foregroundColor;
    const QColor newColor = QColorDialog::getColor(foregroundColor.get(), this);
    if (newColor.isValid() && newColor != foregroundColor.get()) {
        foregroundColor.set(newColor);
        updateFontAndColors();
    }
}

void ClientPage::slot_onChangeColumns(const int value)
{
    setConfig().integratedClient.columns.set(value);
}

void ClientPage::slot_onChangeRows(const int value)
{
    setConfig().integratedClient.rows.set(value);
}

void ClientPage::slot_onChangeLinesOfScrollback(const int value)
{
    setConfig().integratedClient.linesOfScrollback.set(value);
}

void ClientPage::slot_onChangeLinesOfInputHistory(const int value)
{
    setConfig().integratedClient.linesOfInputHistory.set(value);
}

void ClientPage::slot_onChangeTabCompletionDictionarySize(const int value)
{
    setConfig().integratedClient.tabCompletionDictionarySize.set(value);
}
