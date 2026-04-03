// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Ulf Hermann <ulfonk_mennhar@gmx.de> (Alve)
// Author: Marek Krejza <krejza@gmail.com> (Caligor)
// Author: Nils Schimmelmann <nschimme@gmail.com> (Jahara)

#include "generalpage.h"

#include "../configuration/configuration.h"
#include "../global/SignalBlocker.h"
#include "ui_generalpage.h"

#include <QSslSocket>
#include <QString>
#include <QtWidgets>

// Order of entries in charsetComboBox drop down
static_assert(static_cast<int>(CharacterEncodingEnum::LATIN1) == 0);
static_assert(static_cast<int>(CharacterEncodingEnum::UTF8) == 1);
static_assert(static_cast<int>(CharacterEncodingEnum::ASCII) == 2);

// Order of entries in themeComboBox drop down
static_assert(static_cast<int>(ThemeEnum::System) == 0);
static_assert(static_cast<int>(ThemeEnum::Dark) == 1);
static_assert(static_cast<int>(ThemeEnum::Light) == 2);

GeneralPage::GeneralPage(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::GeneralPage)
    , passCfg(this)
{
    ui->setupUi(this);

    auto &cfg = setConfig();

    connect(ui->remoteName, &QLineEdit::textChanged, this, &GeneralPage::slot_remoteNameTextChanged);
    connect(ui->remotePort,
            QOverload<int>::of(&QSpinBox::valueChanged),
            this,
            &GeneralPage::slot_remotePortValueChanged);
    connect(ui->localPort,
            QOverload<int>::of(&QSpinBox::valueChanged),
            this,
            &GeneralPage::slot_localPortValueChanged);
    connect(ui->encryptionCheckBox, &QCheckBox::clicked, this, [](const bool checked) {
        setConfig().connection.tlsEncryption.set(checked);
    });
    connect(ui->proxyListensOnAnyInterfaceCheckBox, &QCheckBox::stateChanged, this, [this]() {
        setConfig().connection.proxyListensOnAnyInterface.set(ui->proxyListensOnAnyInterfaceCheckBox
                                                                ->isChecked());
    });
    connect(ui->charsetComboBox,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            [](const int index) {
                setConfig().general.characterEncoding.set(index);
            });
    connect(ui->themeComboBox,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            &GeneralPage::slot_themeComboBoxChanged);

    connect(ui->emulatedExitsCheckBox,
            &QCheckBox::stateChanged,
            this,
            &GeneralPage::slot_emulatedExitsStateChanged);
    connect(ui->showHiddenExitFlagsCheckBox,
            &QCheckBox::stateChanged,
            this,
            &GeneralPage::slot_showHiddenExitFlagsStateChanged);
    connect(ui->showNotesCheckBox,
            &QCheckBox::stateChanged,
            this,
            &GeneralPage::slot_showNotesStateChanged);

    connect(ui->checkForUpdateCheckBox, &QCheckBox::stateChanged, this, [this]() {
        setConfig().general.checkForUpdate.set(ui->checkForUpdateCheckBox->isChecked());
    });
    connect(ui->autoLoadFileName,
            &QLineEdit::textChanged,
            this,
            &GeneralPage::slot_autoLoadFileNameTextChanged);
    connect(ui->autoLoadCheck,
            &QCheckBox::stateChanged,
            this,
            &GeneralPage::slot_autoLoadCheckStateChanged);
    connect(ui->selectWorldFileButton,
            &QAbstractButton::clicked,
            this,
            &GeneralPage::slot_selectWorldFileButtonClicked);

    connect(ui->displayMumeClockCheckBox,
            &QCheckBox::stateChanged,
            this,
            &GeneralPage::slot_displayMumeClockStateChanged);

    connect(ui->displayXPStatusCheckBox,
            &QCheckBox::stateChanged,
            this,
            &GeneralPage::slot_displayXPStatusStateChanged);

    connect(ui->proxyConnectionStatusCheckBox, &QCheckBox::stateChanged, this, [this]() {
        setConfig().connection.proxyConnectionStatus.set(ui->proxyConnectionStatusCheckBox->isChecked());
    });

    connect(ui->configurationResetButton, &QAbstractButton::clicked, this, [this]() {
        QMessageBox::StandardButton reply
            = QMessageBox::question(this,
                                    "MMapper Factory Reset",
                                    "Are you sure you want to perform a factory reset?",
                                    QMessageBox::Yes | QMessageBox::No);
        if (reply == QMessageBox::Yes) {
            setConfig().reset();
            emit sig_reloadConfig();
        }
    });

    connect(ui->configurationExportButton, &QAbstractButton::clicked, this, []() {
        QTemporaryFile temp(QDir::tempPath() + "/mmapper_XXXXXX.ini");
        temp.setAutoRemove(false);
        if (!temp.open()) {
            qWarning() << "Failed to create temporary file for export";
            return;
        }
        const QString fileName = temp.fileName();
        temp.close();

        {
            QSettings settings(fileName, QSettings::IniFormat);
            getConfig().writeTo(settings);
            settings.sync();
        }

        QFile file(fileName);
        if (file.open(QIODevice::ReadOnly)) {
            const QByteArray content = file.readAll();
            file.close();
            QFileDialog::saveFileContent(content, "mmapper.ini");
        }

        QFile::remove(fileName);
    });

    connect(ui->configurationImportButton, &QAbstractButton::clicked, this, [this]() {
        auto importFile = [this](const QString &fileName,
                                 const std::optional<QByteArray> &fileContent) {
            if (fileName.isEmpty() || !fileContent.has_value()) {
                return;
            }

            const QByteArray &content = fileContent.value();
            if (content.isEmpty()) {
                return;
            }

            QTemporaryFile temp(QDir::tempPath() + "/mmapper_import_XXXXXX.ini");
            temp.setAutoRemove(true);
            if (temp.open()) {
                temp.write(content);
                temp.close();

                {
                    auto &cfg = setConfig();
                    QSettings settings(temp.fileName(), QSettings::IniFormat);
                    cfg.readFrom(settings);
                    cfg.write();
                }
                emit sig_reloadConfig();
            }
        };

        const auto nameFilter = QStringLiteral("Configuration (*.ini);;All files (*)");
        QFileDialog::getOpenFileContent(nameFilter, importFile);
    });

    connect(ui->autoLogin, &QCheckBox::stateChanged, this, [this]() {
        setConfig().account.rememberLogin.set(ui->autoLogin->isChecked());
    });

    connect(ui->accountName, &QLineEdit::textChanged, this, [](const QString &account) {
        setConfig().account.accountName.set(account);
    });

    connect(&passCfg, &PasswordConfig::sig_error, this, [this](const QString &msg) {
        qWarning() << msg;
        QMessageBox::warning(this, "Password Error", msg);
    });

    connect(&passCfg, &PasswordConfig::sig_incomingPassword, this, [this](const QString &password) {
        ui->showPassword->setText("Hide Password");
        ui->accountPassword->setText(password);
        ui->accountPassword->setEchoMode(QLineEdit::Normal);
    });

    connect(ui->accountPassword, &QLineEdit::textEdited, this, [this](const QString &password) {
        setConfig().account.accountPassword.set(!password.isEmpty());
        passCfg.setPassword(password);
    });

    connect(ui->showPassword, &QAbstractButton::clicked, this, [this]() {
        if (ui->showPassword->text() == "Hide Password") {
            ui->showPassword->setText("Show Password");
            ui->accountPassword->clear();
            ui->accountPassword->setEchoMode(QLineEdit::Password);
        } else if (getConfig().account.accountPassword.get() && ui->accountPassword->text().isEmpty()) {
            ui->showPassword->setText("Request Password");
            passCfg.getPassword();
        }
    });

    connect(ui->resourceLineEdit, &QLineEdit::textChanged, this, [](const QString &text) {
        setConfig().canvas.resourcesDirectory.set(text);
    });
    connect(ui->resourcePushButton, &QAbstractButton::clicked, this, [this](bool /*unused*/) {
        const auto resourceDir = getConfig().canvas.resourcesDirectory.get();
        QString directory = QFileDialog::getExistingDirectory(ui->resourcePushButton,
                                                              "Choose resource location ...",
                                                              resourceDir);
        if (!directory.isEmpty()) {
            ui->resourceLineEdit->setText(directory);
            setConfig().canvas.resourcesDirectory.set(directory);
        }
    });

    if constexpr (CURRENT_PLATFORM == PlatformEnum::Wasm) {
        ui->remotePort->setDisabled(true);
        ui->localPort->setDisabled(true);
        ui->charsetComboBox->setDisabled(true);
        ui->proxyListensOnAnyInterfaceCheckBox->setDisabled(true);
        ui->proxyConnectionStatusCheckBox->setDisabled(true);
        ui->resourceLineEdit->setDisabled(true);
        ui->resourcePushButton->setDisabled(true);
    }

    cfg.connection.remoteServerName.registerChangeCallback(m_lifetime, [this]() { slot_loadConfig(); });
    cfg.connection.remotePort.registerChangeCallback(m_lifetime, [this]() { slot_loadConfig(); });
    cfg.connection.localPort.registerChangeCallback(m_lifetime, [this]() { slot_loadConfig(); });
    cfg.connection.tlsEncryption.registerChangeCallback(m_lifetime, [this]() { slot_loadConfig(); });
    cfg.connection.proxyListensOnAnyInterface.registerChangeCallback(m_lifetime, [this]() { slot_loadConfig(); });
    cfg.connection.proxyConnectionStatus.registerChangeCallback(m_lifetime, [this]() { slot_loadConfig(); });
    cfg.general.characterEncoding.registerChangeCallback(m_lifetime, [this]() { slot_loadConfig(); });
    cfg.general.theme.registerChangeCallback(m_lifetime, [this]() { slot_loadConfig(); });
    cfg.general.checkForUpdate.registerChangeCallback(m_lifetime, [this]() { slot_loadConfig(); });
    cfg.mumeNative.emulatedExits.registerChangeCallback(m_lifetime, [this]() { slot_loadConfig(); });
    cfg.mumeNative.showHiddenExitFlags.registerChangeCallback(m_lifetime, [this]() { slot_loadConfig(); });
    cfg.mumeNative.showNotes.registerChangeCallback(m_lifetime, [this]() { slot_loadConfig(); });
    cfg.autoLoad.fileName.registerChangeCallback(m_lifetime, [this]() { slot_loadConfig(); });
    cfg.autoLoad.autoLoadMap.registerChangeCallback(m_lifetime, [this]() { slot_loadConfig(); });
    cfg.mumeClock.display.registerChangeCallback(m_lifetime, [this]() { slot_loadConfig(); });
    cfg.adventurePanel.displayXPStatus.registerChangeCallback(m_lifetime, [this]() { slot_loadConfig(); });
    cfg.account.rememberLogin.registerChangeCallback(m_lifetime, [this]() { slot_loadConfig(); });
    cfg.account.accountName.registerChangeCallback(m_lifetime, [this]() { slot_loadConfig(); });
    cfg.canvas.resourcesDirectory.registerChangeCallback(m_lifetime, [this]() { slot_loadConfig(); });
}

GeneralPage::~GeneralPage()
{
    delete ui;
}

void GeneralPage::slot_loadConfig()
{
    const auto &config = getConfig();
    const auto &connection = config.connection;
    const auto &mumeNative = config.mumeNative;
    const auto &autoLoad = config.autoLoad;
    const auto &general = config.general;
    const auto &account = config.account;

    SignalBlocker b1(*ui->remoteName);
    SignalBlocker b2(*ui->remotePort);
    SignalBlocker b3(*ui->localPort);
    SignalBlocker b4(*ui->encryptionCheckBox);
    SignalBlocker b5(*ui->proxyListensOnAnyInterfaceCheckBox);
    SignalBlocker b6(*ui->charsetComboBox);
    SignalBlocker b7(*ui->themeComboBox);
    SignalBlocker b8(*ui->emulatedExitsCheckBox);
    SignalBlocker b9(*ui->showHiddenExitFlagsCheckBox);
    SignalBlocker b10(*ui->showNotesCheckBox);
    SignalBlocker b11(*ui->checkForUpdateCheckBox);
    SignalBlocker b12(*ui->autoLoadFileName);
    SignalBlocker b13(*ui->autoLoadCheck);
    SignalBlocker b14(*ui->displayMumeClockCheckBox);
    SignalBlocker b15(*ui->displayXPStatusCheckBox);
    SignalBlocker b16(*ui->proxyConnectionStatusCheckBox);
    SignalBlocker b17(*ui->resourceLineEdit);
    SignalBlocker b18(*ui->autoLogin);
    SignalBlocker b19(*ui->accountName);

    ui->remoteName->setText(connection.remoteServerName.get());
    ui->remotePort->setValue(connection.remotePort.get());
    ui->localPort->setValue(connection.localPort.get());
#ifdef Q_OS_WASM
    ui->encryptionCheckBox->setDisabled(true);
    ui->encryptionCheckBox->setChecked(true);
#else
    if (!QSslSocket::supportsSsl() && NO_WEBSOCKET) {
        ui->encryptionCheckBox->setEnabled(false);
        ui->encryptionCheckBox->setChecked(false);
    } else {
        ui->encryptionCheckBox->setChecked(connection.tlsEncryption.get());
    }
#endif
    ui->proxyListensOnAnyInterfaceCheckBox->setChecked(connection.proxyListensOnAnyInterface.get());
    ui->charsetComboBox->setCurrentIndex(general.characterEncoding.get());
    ui->themeComboBox->setCurrentIndex(general.theme.get());

    ui->emulatedExitsCheckBox->setChecked(mumeNative.emulatedExits.get());
    ui->showHiddenExitFlagsCheckBox->setChecked(mumeNative.showHiddenExitFlags.get());
    ui->showNotesCheckBox->setChecked(mumeNative.showNotes.get());

    ui->checkForUpdateCheckBox->setChecked(config.general.checkForUpdate.get());
    ui->checkForUpdateCheckBox->setDisabled(NO_UPDATER);
    ui->autoLoadFileName->setText(autoLoad.fileName.get());
    ui->autoLoadCheck->setChecked(autoLoad.autoLoadMap.get());
    if constexpr (CURRENT_PLATFORM == PlatformEnum::Wasm) {
        ui->autoLoadFileName->setDisabled(true);
        ui->selectWorldFileButton->setDisabled(true);
    } else {
        ui->autoLoadFileName->setEnabled(autoLoad.autoLoadMap.get());
        ui->selectWorldFileButton->setEnabled(autoLoad.autoLoadMap.get());
    }

    ui->displayMumeClockCheckBox->setChecked(config.mumeClock.display.get());

    ui->displayXPStatusCheckBox->setChecked(config.adventurePanel.displayXPStatus.get());

    ui->proxyConnectionStatusCheckBox->setChecked(connection.proxyConnectionStatus.get());

    ui->resourceLineEdit->setText(config.canvas.resourcesDirectory.get());

    if constexpr (NO_QTKEYCHAIN) {
        ui->autoLogin->setEnabled(false);
        ui->accountName->setEnabled(false);
        ui->accountPassword->setEnabled(false);
        ui->showPassword->setEnabled(false);
    } else {
        ui->autoLogin->setChecked(account.rememberLogin.get());
        ui->accountName->setText(account.accountName.get());
        if (!account.accountPassword.get()) {
            ui->accountPassword->setPlaceholderText("");
        }
    }
}

void GeneralPage::slot_selectWorldFileButtonClicked(bool /*unused*/)
{
    // FIXME: code duplication
    const auto savedLastMapDir = getConfig().autoLoad.lastMapDirectory.get();
    QString fileName = QFileDialog::getOpenFileName(this,
                                                    "Choose map file ...",
                                                    savedLastMapDir,
                                                    "MMapper2 (*.mm2);;MMapper (*.map)");
    if (!fileName.isEmpty()) {
        ui->autoLoadFileName->setText(fileName);
        ui->autoLoadCheck->setChecked(true);
        auto &savedAutoLoad = setConfig().autoLoad;
        savedAutoLoad.fileName.set(fileName);
        savedAutoLoad.autoLoadMap.set(true);
    }
}

void GeneralPage::slot_remoteNameTextChanged(const QString & /*unused*/)
{
    setConfig().connection.remoteServerName.set(ui->remoteName->text());
}

void GeneralPage::slot_remotePortValueChanged(int /*unused*/)
{
    setConfig().connection.remotePort.set(ui->remotePort->value());
}

void GeneralPage::slot_localPortValueChanged(int /*unused*/)
{
    setConfig().connection.localPort.set(ui->localPort->value());
}

void GeneralPage::slot_emulatedExitsStateChanged(int /*unused*/)
{
    setConfig().mumeNative.emulatedExits.set(ui->emulatedExitsCheckBox->isChecked());
}

void GeneralPage::slot_showHiddenExitFlagsStateChanged(int /*unused*/)
{
    setConfig().mumeNative.showHiddenExitFlags.set(ui->showHiddenExitFlagsCheckBox->isChecked());
}

void GeneralPage::slot_showNotesStateChanged(int /*unused*/)
{
    setConfig().mumeNative.showNotes.set(ui->showNotesCheckBox->isChecked());
}

void GeneralPage::slot_autoLoadFileNameTextChanged(const QString & /*unused*/)
{
    setConfig().autoLoad.fileName.set(ui->autoLoadFileName->text());
}

void GeneralPage::slot_autoLoadCheckStateChanged(int /*unused*/)
{
    setConfig().autoLoad.autoLoadMap.set(ui->autoLoadCheck->isChecked());
    if (CURRENT_PLATFORM != PlatformEnum::Wasm) {
        ui->autoLoadFileName->setEnabled(ui->autoLoadCheck->isChecked());
        ui->selectWorldFileButton->setEnabled(ui->autoLoadCheck->isChecked());
    }
}

void GeneralPage::slot_displayMumeClockStateChanged(int /*unused*/)
{
    setConfig().mumeClock.display.set(ui->displayMumeClockCheckBox->isChecked());
}

void GeneralPage::slot_displayXPStatusStateChanged([[maybe_unused]] int)
{
    setConfig().adventurePanel.displayXPStatus.set(ui->displayXPStatusCheckBox->isChecked());
}

void GeneralPage::slot_themeComboBoxChanged(int index)
{
    setConfig().general.theme.set(index);
}
