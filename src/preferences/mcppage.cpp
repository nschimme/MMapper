// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "mcppage.h"

#include "../configuration/configuration.h"
#include "../global/SignalBlocker.h"
#include "ui_mcppage.h"

#include <QClipboard>
#include <QGuiApplication>

McpPage::McpPage(QWidget *const parent)
    : QWidget(parent)
    , ui(new Ui::McpPage)
{
    ui->setupUi(this);
    slot_loadConfig();

    // Changing these restarts the server right away; the service listens for the change.
    connect(ui->enabledCheckBox, &QCheckBox::toggled, this, [](const bool enabled) {
        setConfig().mcp.setEnabled(enabled);
    });
    // editingFinished, not valueChanged: typing "4281" must not try ports 4, 42 and 428 on the way.
    connect(ui->portSpinBox, &QSpinBox::editingFinished, this, [this]() {
        setConfig().mcp.setPort(static_cast<uint16_t>(ui->portSpinBox->value()));
    });
    connect(ui->copyTokenButton, &QPushButton::clicked, this, [this]() {
        QGuiApplication::clipboard()->setText(ui->tokenLineEdit->text());
    });
    connect(ui->regenerateTokenButton, &QPushButton::clicked, this, [this]() {
        setConfig().mcp.regenerateToken();
        slot_loadConfig();
    });

    // The server reports what it is doing (listening, or why it could not) through the config.
    setConfig().mcp.registerStatusCallback(m_lifetime, [this]() {
        ui->statusLabel->setText(getConfig().mcp.getStatus());
    });
}

McpPage::~McpPage()
{
    delete ui;
}

void McpPage::slot_loadConfig()
{
    const auto &mcp = getConfig().mcp;
    SignalBlocker enabledBlocker(*ui->enabledCheckBox);
    SignalBlocker portBlocker(*ui->portSpinBox);

    ui->enabledCheckBox->setChecked(mcp.isEnabled());
    ui->portSpinBox->setValue(mcp.getPort());
    ui->tokenLineEdit->setText(mcp.getToken());
    ui->statusLabel->setText(mcp.getStatus());
}
