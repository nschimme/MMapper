// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "generalpage.h"
#include "ui_generalpage.h"
#include "../global/SignalBlocker.h"

GeneralPage::GeneralPage(QWidget *parent)
    : QWidget(parent), ui(new Ui::GeneralPage)
{
    ui->setupUi(this);
    connect(&m_viewModel, &GeneralViewModel::settingsChanged, this, &GeneralPage::updateUI);

    connect(ui->remoteName, &QLineEdit::textChanged, &m_viewModel, &GeneralViewModel::setRemoteName);
    connect(ui->remotePort, QOverload<int>::of(&QSpinBox::valueChanged), &m_viewModel, &GeneralViewModel::setRemotePort);
    connect(ui->localPort, QOverload<int>::of(&QSpinBox::valueChanged), &m_viewModel, &GeneralViewModel::setLocalPort);
    connect(ui->encryptionCheckBox, &QCheckBox::toggled, &m_viewModel, &GeneralViewModel::setTlsEncryption);
    connect(ui->proxyListensOnAnyInterfaceCheckBox, &QCheckBox::toggled, &m_viewModel, &GeneralViewModel::setProxyListensOnAnyInterface);
    connect(ui->charsetComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), &m_viewModel, &GeneralViewModel::setCharacterEncoding);
    connect(ui->themeComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), &m_viewModel, &GeneralViewModel::setTheme);
    connect(ui->emulatedExitsCheckBox, &QCheckBox::toggled, &m_viewModel, &GeneralViewModel::setEmulateExits);
    connect(ui->showHiddenExitFlagsCheckBox, &QCheckBox::toggled, &m_viewModel, &GeneralViewModel::setShowHiddenExitFlags);
    connect(ui->showNotesCheckBox, &QCheckBox::toggled, &m_viewModel, &GeneralViewModel::setShowNotes);
    connect(ui->checkForUpdateCheckBox, &QCheckBox::toggled, &m_viewModel, &GeneralViewModel::setCheckForUpdate);
    connect(ui->autoLoadFileName, &QLineEdit::textChanged, &m_viewModel, &GeneralViewModel::setAutoLoadFileName);
    connect(ui->autoLoadCheck, &QCheckBox::toggled, &m_viewModel, &GeneralViewModel::setAutoLoadMap);
    connect(ui->displayMumeClockCheckBox, &QCheckBox::toggled, &m_viewModel, &GeneralViewModel::setDisplayMumeClock);
    connect(ui->displayXPStatusCheckBox, &QCheckBox::toggled, &m_viewModel, &GeneralViewModel::setDisplayXPStatus);
    connect(ui->proxyConnectionStatusCheckBox, &QCheckBox::toggled, &m_viewModel, &GeneralViewModel::setProxyConnectionStatus);
    connect(ui->autoLogin, &QCheckBox::toggled, &m_viewModel, &GeneralViewModel::setRememberLogin);
    connect(ui->accountName, &QLineEdit::textChanged, &m_viewModel, &GeneralViewModel::setAccountName);

    updateUI();
}

GeneralPage::~GeneralPage() = default;

void GeneralPage::updateUI()
{
    SignalBlocker sb1(*ui->remoteName);
    SignalBlocker sb2(*ui->remotePort);
    SignalBlocker sb3(*ui->localPort);
    SignalBlocker sb4(*ui->encryptionCheckBox);
    SignalBlocker sb5(*ui->proxyListensOnAnyInterfaceCheckBox);
    SignalBlocker sb6(*ui->charsetComboBox);
    SignalBlocker sb7(*ui->themeComboBox);
    SignalBlocker sb8(*ui->emulatedExitsCheckBox);
    SignalBlocker sb9(*ui->showHiddenExitFlagsCheckBox);
    SignalBlocker sb10(*ui->showNotesCheckBox);
    SignalBlocker sb11(*ui->checkForUpdateCheckBox);
    SignalBlocker sb12(*ui->autoLoadFileName);
    SignalBlocker sb13(*ui->autoLoadCheck);
    SignalBlocker sb14(*ui->displayMumeClockCheckBox);
    SignalBlocker sb15(*ui->displayXPStatusCheckBox);
    SignalBlocker sb16(*ui->proxyConnectionStatusCheckBox);
    SignalBlocker sb17(*ui->autoLogin);
    SignalBlocker sb18(*ui->accountName);

    ui->remoteName->setText(m_viewModel.remoteName());
    ui->remotePort->setValue(m_viewModel.remotePort());
    ui->localPort->setValue(m_viewModel.localPort());
    ui->encryptionCheckBox->setChecked(m_viewModel.tlsEncryption());
    ui->proxyListensOnAnyInterfaceCheckBox->setChecked(m_viewModel.proxyListensOnAnyInterface());
    ui->charsetComboBox->setCurrentIndex(m_viewModel.characterEncoding());
    ui->themeComboBox->setCurrentIndex(m_viewModel.theme());
    ui->emulatedExitsCheckBox->setChecked(m_viewModel.emulateExits());
    ui->showHiddenExitFlagsCheckBox->setChecked(m_viewModel.showHiddenExitFlags());
    ui->showNotesCheckBox->setChecked(m_viewModel.showNotes());
    ui->checkForUpdateCheckBox->setChecked(m_viewModel.checkForUpdate());
    ui->autoLoadFileName->setText(m_viewModel.autoLoadFileName());
    ui->autoLoadCheck->setChecked(m_viewModel.autoLoadMap());
    ui->displayMumeClockCheckBox->setChecked(m_viewModel.displayMumeClock());
    ui->displayXPStatusCheckBox->setChecked(m_viewModel.displayXPStatus());
    ui->proxyConnectionStatusCheckBox->setChecked(m_viewModel.proxyConnectionStatus());
    ui->autoLogin->setChecked(m_viewModel.rememberLogin());
    ui->accountName->setText(m_viewModel.accountName());
}

void GeneralPage::slot_loadConfig() { m_viewModel.loadConfig(); }
