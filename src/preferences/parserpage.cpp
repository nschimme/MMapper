// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "parserpage.h"
#include "ui_parserpage.h"
#include "AnsiColorDialog.h"
#include "ansicombo.h"
#include "../global/SignalBlocker.h"

ParserPage::ParserPage(QWidget *parent)
    : QWidget(parent), ui(new Ui::ParserPage)
{
    ui->setupUi(this);
    connect(&m_viewModel, &ParserPageViewModel::settingsChanged, this, &ParserPage::updateUI);

    connect(ui->roomNameColorPushButton, &QPushButton::clicked, this, [this]() {
        AnsiColorDialog::getColor(m_viewModel.roomNameColor(), this, [this](QString ansi) {
            m_viewModel.setRoomNameColor(ansi);
        });
    });
    connect(ui->roomDescColorPushButton, &QPushButton::clicked, this, [this]() {
        AnsiColorDialog::getColor(m_viewModel.roomDescColor(), this, [this](QString ansi) {
            m_viewModel.setRoomDescColor(ansi);
        });
    });
    connect(ui->charPrefixLineEdit, &QLineEdit::textChanged, &m_viewModel, &ParserPageViewModel::setPrefixChar);
    connect(ui->encodeEmoji, &QCheckBox::toggled, &m_viewModel, &ParserPageViewModel::setEncodeEmoji);
    connect(ui->decodeEmoji, &QCheckBox::toggled, &m_viewModel, &ParserPageViewModel::setDecodeEmoji);

    updateUI();
}

ParserPage::~ParserPage() = default;

void ParserPage::updateUI()
{
    SignalBlocker sb1(*ui->charPrefixLineEdit);
    SignalBlocker sb2(*ui->encodeEmoji);
    SignalBlocker sb3(*ui->decodeEmoji);

    AnsiCombo::makeWidgetColoured(ui->roomNameColorLabel, m_viewModel.roomNameColor());
    AnsiCombo::makeWidgetColoured(ui->roomDescColorLabel, m_viewModel.roomDescColor());

    ui->charPrefixLineEdit->setText(m_viewModel.prefixChar());
    ui->encodeEmoji->setChecked(m_viewModel.encodeEmoji());
    ui->decodeEmoji->setChecked(m_viewModel.decodeEmoji());
}

void ParserPage::slot_loadConfig()
{
    m_viewModel.loadConfig();
}
