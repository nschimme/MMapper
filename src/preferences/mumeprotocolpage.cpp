// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "mumeprotocolpage.h"
#include "ui_mumeprotocolpage.h"
#include "../global/SignalBlocker.h"

MumeProtocolPage::MumeProtocolPage(QWidget *parent)
    : QWidget(parent), ui(new Ui::MumeProtocolPage)
{
    ui->setupUi(this);
    connect(&m_viewModel, &MumeProtocolViewModel::settingsChanged, this, &MumeProtocolPage::updateUI);

    connect(ui->internalEditorRadioButton, &QRadioButton::toggled, this, [this](bool checked) {
        if (checked) m_viewModel.setUseInternalEditor(true);
    });
    connect(ui->externalEditorRadioButton, &QRadioButton::toggled, this, [this](bool checked) {
        if (checked) m_viewModel.setUseInternalEditor(false);
    });
    connect(ui->externalEditorCommand, &QLineEdit::textChanged, &m_viewModel, &MumeProtocolViewModel::setExternalEditorCommand);

    updateUI();
}

MumeProtocolPage::~MumeProtocolPage() = default;

void MumeProtocolPage::updateUI()
{
    SignalBlocker sb1(*ui->internalEditorRadioButton);
    SignalBlocker sb2(*ui->externalEditorRadioButton);
    SignalBlocker sb3(*ui->externalEditorCommand);

    if (m_viewModel.useInternalEditor()) {
        ui->internalEditorRadioButton->setChecked(true);
    } else {
        ui->externalEditorRadioButton->setChecked(true);
    }
    ui->externalEditorCommand->setText(m_viewModel.externalEditorCommand());
}

void MumeProtocolPage::slot_loadConfig()
{
    m_viewModel.loadConfig();
}
