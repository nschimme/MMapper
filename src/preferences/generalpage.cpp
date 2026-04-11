// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Ulf Hermann <ulfonk_mennhar@gmx.de> (Alve)
// Author: Marek Krejza <krejza@gmail.com> (Caligor)
// Author: Nils Schimmelmann <nschimme@gmail.com> (Jahara)

#include "generalpage.h"

#include "../configuration/configuration.h"
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

GeneralPage::GeneralPage(QWidget *parent, Configuration &config)
    : QWidget(parent)
    , ui(new Ui::GeneralPage)
    , m_config(config)
    , passCfg(this)
{
    ui->setupUi(this);

    connect(ui->remoteName, &QLineEdit::textChanged, this, &GeneralPage::slot_remoteNameTextChanged);
    connect(ui->remotePort,
            QOverload<int>::of(&QSpinBox::valueChanged),
            this,
            &GeneralPage::slot_remotePortValueChanged);
    connect(ui->localPort,
            QOverload<int>::of(&QSpinBox::valueChanged),
            this,
            &GeneralPage::slot_localPortValueChanged);
    connect(ui->encryptionCheckBox, &QCheckBox::clicked, this, [this](const bool checked) {
        m_config.connection.tlsEncryption = checked;
    });
    connect(ui->proxyListensOnAnyInterfaceCheckBox, &QCheckBox::toggled, this, [this]() {
        m_config.connection.proxyListensOnAnyInterface = ui->proxyListensOnAnyInterfaceCheckBox
                                                             ->isChecked();
    });
    connect(ui->charsetComboBox,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            [this](const int index) {
                m_config.general.characterEncoding = static_cast<CharacterEncodingEnum>(index);
            });
    connect(ui->themeComboBox,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            &GeneralPage::slot_themeComboBoxChanged);

    connect(ui->emulatedExitsCheckBox,
            &QCheckBox::toggled,
            this,
            &GeneralPage::slot_emulatedExitsStateChanged);
    connect(ui->showHiddenExitFlagsCheckBox,
            &QCheckBox::toggled,
            this,
            &GeneralPage::slot_showHiddenExitFlagsStateChanged);
    connect(ui->showNotesCheckBox,
            &QCheckBox::toggled,
            this,
            &GeneralPage::slot_showNotesStateChanged);

    connect(ui->checkForUpdateCheckBox, &QCheckBox::toggled, this, [this]() {
        m_config.general.checkForUpdate = ui->checkForUpdateCheckBox->isChecked();
    });
    connect(ui->autoLoadFileName,
            &QLineEdit::textChanged,
            this,
            &GeneralPage::slot_autoLoadFileNameTextChanged);
    connect(ui->autoLoadCheck,
            &QCheckBox::toggled,
            this,
            &GeneralPage::slot_autoLoadCheckStateChanged);
    connect(ui->selectWorldFileButton,
            &QAbstractButton::clicked,
            this,
            &GeneralPage::slot_selectWorldFileButtonClicked);

    connect(ui->displayMumeClockCheckBox,
            &QCheckBox::toggled,
            this,
            &GeneralPage::slot_displayMumeClockStateChanged);

    connect(ui->displayXPStatusCheckBox,
            &QCheckBox::toggled,
            this,
            &GeneralPage::slot_displayXPStatusStateChanged);

    connect(ui->proxyConnectionStatusCheckBox, &QCheckBox::toggled, this, [this]() {
        m_config.connection.proxyConnectionStatus = ui->proxyConnectionStatusCheckBox->isChecked();
    });

    connect(ui->configurationResetButton, &QAbstractButton::clicked, this, [this]() {
        QMessageBox::StandardButton reply
            = QMessageBox::question(this,
                                    "MMapper Factory Reset",
                                    "Are you sure you want to perform a factory reset?",
                                    QMessageBox::Yes | QMessageBox::No);
        if (reply == QMessageBox::Yes) {
            setConfig().reset();
            // m_config = getConfig(); // ConfigDialog will update this via sig_reloadConfig
            emit sig_reloadConfig();
        }
    });

    connect(ui->configurationExportButton, &QAbstractButton::clicked, this, [this]() {
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
            m_config.writeTo(settings);
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
                    cfg.read(); // Refresh from disk to ensure all internal state is consistent
                }
                emit sig_reloadConfig();
            }
        };

        const auto nameFilter = QStringLiteral("Configuration (*.ini);;All files (*)");
        QFileDialog::getOpenFileContent(nameFilter, importFile);
    });

    connect(ui->autoLogin, &QCheckBox::toggled, this, [this]() {
        m_config.account.rememberLogin = ui->autoLogin->isChecked();
    });

    connect(ui->accountName, &QLineEdit::textChanged, this, [this](const QString &account) {
        m_config.account.accountName = account;
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
        m_config.account.accountPassword = !password.isEmpty();
        passCfg.setPassword(password);
    });

    connect(ui->showPassword, &QAbstractButton::clicked, this, [this]() {
        if (ui->showPassword->text() == "Hide Password") {
            ui->showPassword->setText("Show Password");
            ui->accountPassword->clear();
            ui->accountPassword->setEchoMode(QLineEdit::Password);
        } else if (m_config.account.accountPassword && ui->accountPassword->text().isEmpty()) {
            ui->showPassword->setText("Request Password");
            passCfg.getPassword();
        }
    });

    connect(ui->resourceLineEdit, &QLineEdit::textChanged, this, [this](const QString &text) {
        m_config.canvas.resourcesDirectory = text;
    });
    connect(ui->resourcePushButton, &QAbstractButton::clicked, this, [this](bool /*unused*/) {
        const auto &resourceDir = m_config.canvas.resourcesDirectory;
        QString directory = QFileDialog::getExistingDirectory(ui->resourcePushButton,
                                                              "Choose resource location ...",
                                                              resourceDir);
        if (!directory.isEmpty()) {
            ui->resourceLineEdit->setText(directory);
            m_config.canvas.resourcesDirectory = directory;
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
}

GeneralPage::~GeneralPage()
{
    delete ui;
}

void GeneralPage::slot_loadConfig()
{
    const auto &config = m_config;
    const auto &connection = config.connection;
    const auto &mumeNative = config.mumeNative;
    const auto &autoLoad = config.autoLoad;
    const auto &general = config.general;
    const auto &account = config.account;

    ui->remoteName->setText(connection.remoteServerName);
    ui->remotePort->setValue(connection.remotePort);
    ui->localPort->setValue(connection.localPort);
#ifdef Q_OS_WASM
    ui->encryptionCheckBox->setDisabled(true);
    ui->encryptionCheckBox->setChecked(true);
#else
    if (!QSslSocket::supportsSsl() && NO_WEBSOCKET) {
        ui->encryptionCheckBox->setEnabled(false);
        ui->encryptionCheckBox->setChecked(false);
    } else {
        ui->encryptionCheckBox->setChecked(connection.tlsEncryption);
    }
#endif
    ui->proxyListensOnAnyInterfaceCheckBox->setChecked(connection.proxyListensOnAnyInterface);
    ui->charsetComboBox->setCurrentIndex(static_cast<int>(general.characterEncoding.get()));
    ui->themeComboBox->setCurrentIndex(static_cast<int>(general.getTheme()));

    ui->emulatedExitsCheckBox->setChecked(mumeNative.emulatedExits);
    ui->showHiddenExitFlagsCheckBox->setChecked(mumeNative.showHiddenExitFlags);
    ui->showNotesCheckBox->setChecked(mumeNative.showNotes);

    ui->checkForUpdateCheckBox->setChecked(config.general.checkForUpdate);
    ui->checkForUpdateCheckBox->setDisabled(NO_UPDATER);
    ui->autoLoadFileName->setText(autoLoad.fileName);
    ui->autoLoadCheck->setChecked(autoLoad.autoLoadMap);
    if constexpr (CURRENT_PLATFORM == PlatformEnum::Wasm) {
        ui->autoLoadFileName->setDisabled(true);
        ui->selectWorldFileButton->setDisabled(true);
    } else {
        ui->autoLoadFileName->setEnabled(autoLoad.autoLoadMap);
        ui->selectWorldFileButton->setEnabled(autoLoad.autoLoadMap);
    }

    ui->displayMumeClockCheckBox->setChecked(config.mumeClock.display);

    ui->displayXPStatusCheckBox->setChecked(config.adventurePanel.getDisplayXPStatus());

    ui->proxyConnectionStatusCheckBox->setChecked(connection.proxyConnectionStatus);

    ui->resourceLineEdit->setText(config.canvas.resourcesDirectory);

    if constexpr (NO_QTKEYCHAIN) {
        ui->autoLogin->setEnabled(false);
        ui->accountName->setEnabled(false);
        ui->accountPassword->setEnabled(false);
        ui->showPassword->setEnabled(false);
    } else {
        ui->autoLogin->setChecked(account.rememberLogin);
        ui->accountName->setText(account.accountName);
        if (!account.accountPassword) {
            ui->accountPassword->setPlaceholderText("");
        }
    }
}

void GeneralPage::slot_selectWorldFileButtonClicked(bool /*unused*/)
{
    // FIXME: code duplication
    const auto &savedLastMapDir = m_config.autoLoad.lastMapDirectory;
    QString fileName = QFileDialog::getOpenFileName(this,
                                                    "Choose map file ...",
                                                    savedLastMapDir,
                                                    "MMapper2 (*.mm2);;MMapper (*.map)");
    if (!fileName.isEmpty()) {
        ui->autoLoadFileName->setText(fileName);
        ui->autoLoadCheck->setChecked(true);
        auto &savedAutoLoad = m_config.autoLoad;
        savedAutoLoad.fileName = fileName;
        savedAutoLoad.autoLoadMap = true;
    }
}

void GeneralPage::slot_remoteNameTextChanged(const QString & /*unused*/)
{
    m_config.connection.remoteServerName = ui->remoteName->text();
}

void GeneralPage::slot_remotePortValueChanged(int /*unused*/)
{
    m_config.connection.remotePort = static_cast<uint16_t>(ui->remotePort->value());
}

void GeneralPage::slot_localPortValueChanged(int /*unused*/)
{
    m_config.connection.localPort = static_cast<uint16_t>(ui->localPort->value());
}

void GeneralPage::slot_emulatedExitsStateChanged(bool /*unused*/)
{
    m_config.mumeNative.emulatedExits = ui->emulatedExitsCheckBox->isChecked();
}

void GeneralPage::slot_showHiddenExitFlagsStateChanged(bool /*unused*/)
{
    m_config.mumeNative.showHiddenExitFlags = ui->showHiddenExitFlagsCheckBox->isChecked();
}

void GeneralPage::slot_showNotesStateChanged(bool /*unused*/)
{
    m_config.mumeNative.showNotes = ui->showNotesCheckBox->isChecked();
}

void GeneralPage::slot_autoLoadFileNameTextChanged(const QString & /*unused*/)
{
    m_config.autoLoad.fileName = ui->autoLoadFileName->text();
}

void GeneralPage::slot_autoLoadCheckStateChanged(bool /*unused*/)
{
    m_config.autoLoad.autoLoadMap = ui->autoLoadCheck->isChecked();
    if (CURRENT_PLATFORM != PlatformEnum::Wasm) {
        ui->autoLoadFileName->setEnabled(ui->autoLoadCheck->isChecked());
        ui->selectWorldFileButton->setEnabled(ui->autoLoadCheck->isChecked());
    }
}

void GeneralPage::slot_displayMumeClockStateChanged(bool /*unused*/)
{
    m_config.mumeClock.display = ui->displayMumeClockCheckBox->isChecked();
}

void GeneralPage::slot_displayXPStatusStateChanged([[maybe_unused]] bool)
{
    m_config.adventurePanel.setDisplayXPStatus(ui->displayXPStatusCheckBox->isChecked());
}

void GeneralPage::slot_themeComboBoxChanged(int index)
{
    m_config.general.setTheme(static_cast<ThemeEnum>(index));
}
