// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Nils Schimmelmann <nschimme@gmail.com> (Jahara)

#include "clientpage.h"

#include "../configuration/configuration.h"
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

ClientPage::ClientPage(QWidget *parent, Configuration &config)
    : QWidget(parent)
    , ui(new Ui::ClientPage)
    , m_config(config)
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
            [this](const int value) { m_config.integratedClient.linesOfPeekPreview = value; });

    connect(ui->inputHistorySpinBox,
            QOverload<int>::of(&QSpinBox::valueChanged),
            this,
            &ClientPage::slot_onChangeLinesOfInputHistory);
    connect(ui->tabDictionarySpinBox,
            QOverload<int>::of(&QSpinBox::valueChanged),
            this,
            &ClientPage::slot_onChangeTabCompletionDictionarySize);

    connect(ui->clearInputCheckBox, &QCheckBox::toggled, [this](bool isChecked) {
        m_config.integratedClient.clearInputOnEnter = isChecked;
    });

    connect(ui->autoResizeTerminalCheckBox, &QCheckBox::toggled, [this](bool isChecked) {
        m_config.integratedClient.autoResizeTerminal = isChecked;
    });

    connect(ui->audibleBellCheckBox, &QCheckBox::toggled, [this](bool isChecked) {
        m_config.integratedClient.audibleBell = isChecked;
    });

    connect(ui->visualBellCheckBox, &QCheckBox::toggled, [this](bool isChecked) {
        m_config.integratedClient.visualBell = isChecked;
    });

    connect(ui->commandSeparatorCheckBox, &QCheckBox::toggled, this, [this](bool isChecked) {
        m_config.integratedClient.useCommandSeparator = isChecked;
        ui->commandSeparatorLineEdit->setEnabled(isChecked);
    });

    connect(ui->commandSeparatorLineEdit, &QLineEdit::textChanged, this, [this](const QString &text) {
        if (text.length() == 1) {
            m_config.integratedClient.commandSeparator = text;
        }
    });

    ui->commandSeparatorLineEdit->setValidator(new CustomSeparatorValidator(this));
}

ClientPage::~ClientPage()
{
    delete ui;
}

void ClientPage::slot_loadConfig()
{
    updateFontAndColors();

    const auto &settings = m_config.integratedClient;
    ui->columnsSpinBox->setValue(settings.columns);
    ui->rowsSpinBox->setValue(settings.rows);
    ui->scrollbackSpinBox->setValue(settings.linesOfScrollback);
    ui->previewSpinBox->setValue(settings.linesOfPeekPreview);
    ui->inputHistorySpinBox->setValue(settings.linesOfInputHistory);
    ui->tabDictionarySpinBox->setValue(settings.tabCompletionDictionarySize);
    ui->clearInputCheckBox->setChecked(settings.clearInputOnEnter);
    ui->autoResizeTerminalCheckBox->setChecked(settings.autoResizeTerminal);
    ui->audibleBellCheckBox->setChecked(settings.audibleBell);
    ui->visualBellCheckBox->setChecked(settings.visualBell);
    ui->commandSeparatorCheckBox->setChecked(settings.useCommandSeparator);
    ui->commandSeparatorLineEdit->setText(settings.commandSeparator);
    ui->commandSeparatorLineEdit->setEnabled(settings.useCommandSeparator);
}

void ClientPage::updateFontAndColors()
{
    const auto &settings = m_config.integratedClient;
    QFont font;
    font.fromString(settings.font);
    ui->exampleLabel->setFont(font);

    QFontInfo fi(font);
    ui->fontPushButton->setText(
        QString("%1 %2, %3").arg(fi.family()).arg(fi.styleName()).arg(fi.pointSize()));

    QPixmap fgPix(16, 16);
    fgPix.fill(settings.foregroundColor);
    ui->fgColorPushButton->setIcon(QIcon(fgPix));

    QPixmap bgPix(16, 16);
    bgPix.fill(settings.backgroundColor);
    ui->bgColorPushButton->setIcon(QIcon(bgPix));

    QPalette palette = ui->exampleLabel->palette();
    ui->exampleLabel->setAutoFillBackground(true);
    palette.setColor(QPalette::WindowText, settings.foregroundColor);
    palette.setColor(QPalette::Window, settings.backgroundColor);
    ui->exampleLabel->setPalette(palette);
    ui->exampleLabel->setBackgroundRole(QPalette::Window);
}

void ClientPage::slot_onChangeFont()
{
    auto &fontDescription = m_config.integratedClient.font;
    QFont oldFont;
    oldFont.fromString(fontDescription);

    bool ok = false;
    const QFont newFont = QFontDialog::getFont(&ok,
                                               oldFont,
                                               this,
                                               "Select Font",
                                               QFontDialog::MonospacedFonts);
    if (ok) {
        fontDescription = newFont.toString();
        updateFontAndColors();
        emit sig_changed();
    }
}

void ClientPage::slot_onChangeBackgroundColor()
{
    auto &backgroundColor = m_config.integratedClient.backgroundColor;
    const QColor newColor = QColorDialog::getColor(backgroundColor, this);
    if (newColor.isValid() && newColor != backgroundColor) {
        backgroundColor = newColor;
        updateFontAndColors();
        emit sig_changed();
    }
}

void ClientPage::slot_onChangeForegroundColor()
{
    auto &foregroundColor = m_config.integratedClient.foregroundColor;
    const QColor newColor = QColorDialog::getColor(foregroundColor, this);
    if (newColor.isValid() && newColor != foregroundColor) {
        foregroundColor = newColor;
        updateFontAndColors();
        emit sig_changed();
    }
}

void ClientPage::slot_onChangeColumns(const int value)
{
    m_config.integratedClient.columns = value;
    emit sig_changed();
}

void ClientPage::slot_onChangeRows(const int value)
{
    m_config.integratedClient.rows = value;
    emit sig_changed();
}

void ClientPage::slot_onChangeLinesOfScrollback(const int value)
{
    m_config.integratedClient.linesOfScrollback = value;
    emit sig_changed();
}

void ClientPage::slot_onChangeLinesOfInputHistory(const int value)
{
    m_config.integratedClient.linesOfInputHistory = value;
    emit sig_changed();
}

void ClientPage::slot_onChangeTabCompletionDictionarySize(const int value)
{
    m_config.integratedClient.tabCompletionDictionarySize = value;
    emit sig_changed();
}
