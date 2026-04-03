// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Nils Schimmelmann <nschimme@gmail.com> (Jahara)

#include "mumeprotocolpage.h"

#include "../configuration/configuration.h"
#include "../global/SignalBlocker.h"
#include "ui_mumeprotocolpage.h"

#include <utility>

#include <QString>
#include <QtWidgets>

MumeProtocolPage::MumeProtocolPage(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::MumeProtocolPage)
{
    ui->setupUi(this);

    connect(ui->internalEditorRadioButton,
            &QAbstractButton::toggled,
            this,
            &MumeProtocolPage::slot_internalEditorRadioButtonChanged);
    connect(ui->externalEditorCommand,
            &QLineEdit::textChanged,
            this,
            &MumeProtocolPage::slot_externalEditorCommandTextChanged);
    connect(ui->externalEditorBrowseButton,
            &QAbstractButton::clicked,
            this,
            &MumeProtocolPage::slot_externalEditorBrowseButtonClicked);

    auto &proto = setConfig().mumeClientProtocol;
    proto.internalRemoteEditor.registerChangeCallback(m_lifetime, [this]() { slot_loadConfig(); });
    proto.externalRemoteEditorCommand.registerChangeCallback(m_lifetime, [this]() { slot_loadConfig(); });
}

MumeProtocolPage::~MumeProtocolPage()
{
    delete ui;
}

void MumeProtocolPage::slot_loadConfig()
{
    const auto &settings = getConfig().mumeClientProtocol;

    SignalBlocker b1(*ui->internalEditorRadioButton);
    SignalBlocker b2(*ui->externalEditorRadioButton);
    SignalBlocker b3(*ui->externalEditorCommand);

    ui->internalEditorRadioButton->setChecked(settings.internalRemoteEditor.get());
    ui->externalEditorRadioButton->setChecked(!settings.internalRemoteEditor.get());
    ui->externalEditorCommand->setText(settings.externalRemoteEditorCommand.get());
    ui->externalEditorCommand->setEnabled(!settings.internalRemoteEditor.get());
    ui->externalEditorBrowseButton->setEnabled(!settings.internalRemoteEditor.get());

    if constexpr (CURRENT_PLATFORM == PlatformEnum::Wasm) {
        ui->externalEditorRadioButton->setDisabled(true);
    }
}

void MumeProtocolPage::slot_internalEditorRadioButtonChanged(bool /*unused*/)
{
    const bool useInternalEditor = ui->internalEditorRadioButton->isChecked();

    setConfig().mumeClientProtocol.internalRemoteEditor.set(useInternalEditor);

    ui->externalEditorCommand->setEnabled(!useInternalEditor);
    ui->externalEditorBrowseButton->setEnabled(!useInternalEditor);
}

void MumeProtocolPage::slot_externalEditorCommandTextChanged(QString text)
{
    setConfig().mumeClientProtocol.externalRemoteEditorCommand.set(std::move(text));
}

void MumeProtocolPage::slot_externalEditorBrowseButtonClicked(bool /*unused*/)
{
    auto &proto = setConfig().mumeClientProtocol;
    QFileInfo dirInfo(proto.externalRemoteEditorCommand.get());
    const auto dir = dirInfo.exists() ? dirInfo.absoluteDir().absolutePath() : QDir::homePath();
    QString fileName = QFileDialog::getOpenFileName(this, "Choose editor...", dir, "Editor (*)");
    if (!fileName.isEmpty()) {
        QString quotedFileName = QString(R"("%1")").arg(fileName.replace(R"(")", R"(\")"));
        ui->externalEditorCommand->setText(quotedFileName);
        proto.externalRemoteEditorCommand.set(quotedFileName);
    }
}
