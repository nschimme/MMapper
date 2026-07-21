// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "GeneralPageAdapter.h"

#include "../configuration/configuration.h"
#include "../global/ConfigConsts.h"

#include <optional>

#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QMessageBox>
#include <QSettings>
#ifndef Q_OS_WASM
#include <QSslSocket>
#endif
#include <QTemporaryFile>

#ifndef MMAPPER_WITH_QML
#include "ManagePasswordDialog.h"
#endif

// Order of entries in characterEncodingIndex / themeIndex; mirrors the
// static_asserts at the top of generalpage.cpp.
static_assert(static_cast<int>(CharacterEncodingEnum::LATIN1) == 0);
static_assert(static_cast<int>(CharacterEncodingEnum::UTF8) == 1);
static_assert(static_cast<int>(CharacterEncodingEnum::ASCII) == 2);
static_assert(static_cast<int>(ThemeEnum::System) == 0);
static_assert(static_cast<int>(ThemeEnum::Dark) == 1);
static_assert(static_cast<int>(ThemeEnum::Light) == 2);

GeneralPageAdapter::GeneralPageAdapter(QWidget *const dialogParent, QObject *const parent)
    : QObject(parent)
    , m_dialogParent(dialogParent)
    , m_passwordConfig(this)
{
#ifndef MMAPPER_WITH_QML
    // See the PasswordDialogLauncher doc comment in GeneralPageAdapter.h:
    // under MMAPPER_WITH_QML, MainWindow overrides this via
    // setPasswordDialogLauncher() once it constructs the QmlDialog-hosting
    // equivalent; this .cpp can't do that itself (see the class doc comment
    // for why).
    m_passwordDialogLauncher = [](const QString &accountName,
                                  const QString &password,
                                  bool /*hasStoredPassword*/,
                                  QWidget *const parent,
                                  std::function<void(const QString &, const QString &)> onAccepted,
                                  std::function<void()> onDeleteRequested) {
        auto *dlg = new ManagePasswordDialog(parent);
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        dlg->setAccountName(accountName);
        if (!password.isEmpty()) {
            dlg->setPassword(password);
        }
        connect(dlg, &ManagePasswordDialog::sig_deleteRequested, dlg, [onDeleteRequested]() {
            onDeleteRequested();
        });
        connect(dlg, &QDialog::accepted, dlg, [dlg, onAccepted]() {
            onAccepted(dlg->accountName(), dlg->password());
        });
        dlg->open();
    };
#endif

    connect(&m_passwordConfig, &PasswordConfig::sig_error, this, [this](const QString &msg) {
        qWarning() << msg;
        auto *box = new QMessageBox(QMessageBox::Warning,
                                    tr("Password Error"),
                                    msg,
                                    QMessageBox::Ok,
                                    m_dialogParent);
        box->setAttribute(Qt::WA_DeleteOnClose);
        box->open();
    });

    connect(&m_passwordConfig,
            &PasswordConfig::sig_incomingPassword,
            this,
            [this](const QString &password) {
                if (!m_passwordRequesting) {
                    return;
                }
                m_passwordRequesting = false;
                const QString accountName = getConfig().account.accountName;
                m_passwordDialogLauncher(
                    accountName,
                    password,
                    /*hasStoredPassword=*/true,
                    m_dialogParent,
                    [this](const QString &newAccountName, const QString &newPassword) {
                        if (!newPassword.isEmpty()) {
                            setConfig().account.accountName = newAccountName;
                            m_passwordConfig.setPassword(newPassword);
                        }
                    },
                    [this]() { m_passwordConfig.deletePassword(); });
            });

    connect(&m_passwordConfig, &PasswordConfig::sig_passwordSaved, this, [this]() {
        setConfig().account.accountPassword = true;
        emit sig_changed();

        if (!getConfig().account.rememberLogin) {
            auto *box = new QMessageBox(QMessageBox::Question,
                                        tr("Login"),
                                        tr("A password was saved. Would you like to also enable "
                                           "automatic login for this account?"),
                                        QMessageBox::Yes | QMessageBox::No,
                                        m_dialogParent);
            box->setAttribute(Qt::WA_DeleteOnClose);
            connect(box, &QMessageBox::finished, this, [this](int result) {
                if (result == QMessageBox::Yes) {
                    setConfig().account.rememberLogin = true;
                    emit sig_changed();
                }
            });
            box->open();
        }
    });

    connect(&m_passwordConfig, &PasswordConfig::sig_passwordDeleted, this, [this]() {
        auto &account = setConfig().account;
        account.accountPassword = false;
        account.rememberLogin = false;
        emit sig_changed();
    });
}

QString GeneralPageAdapter::getRemoteName() const
{
    return getConfig().connection.remoteServerName;
}

void GeneralPageAdapter::setRemoteName(const QString &value)
{
    setConfig().connection.remoteServerName = value;
    emit sig_changed();
}

int GeneralPageAdapter::getRemotePort() const
{
    return getConfig().connection.remotePort;
}

void GeneralPageAdapter::setRemotePort(const int value)
{
    setConfig().connection.remotePort = static_cast<uint16_t>(value);
    emit sig_changed();
}

int GeneralPageAdapter::getLocalPort() const
{
    return getConfig().connection.localPort;
}

void GeneralPageAdapter::setLocalPort(const int value)
{
    setConfig().connection.localPort = static_cast<uint16_t>(value);
    emit sig_changed();
}

bool GeneralPageAdapter::getTlsEncryption() const
{
    return getConfig().connection.tlsEncryption;
}

void GeneralPageAdapter::setTlsEncryption(const bool value)
{
    setConfig().connection.tlsEncryption = value;
    emit sig_changed();
}

bool GeneralPageAdapter::getTlsAvailable()
{
#ifdef Q_OS_WASM
    // Qt for WebAssembly is built without the ssl feature, so QSslSocket
    // does not exist there; websockets provide the encrypted transport.
    return !NO_WEBSOCKET;
#else
    return QSslSocket::supportsSsl() || !NO_WEBSOCKET;
#endif
}

bool GeneralPageAdapter::getProxyListensOnAnyInterface() const
{
    return getConfig().connection.proxyListensOnAnyInterface;
}

void GeneralPageAdapter::setProxyListensOnAnyInterface(const bool value)
{
    setConfig().connection.proxyListensOnAnyInterface = value;
    emit sig_changed();
}

bool GeneralPageAdapter::getProxyConnectionStatus() const
{
    return getConfig().connection.proxyConnectionStatus;
}

void GeneralPageAdapter::setProxyConnectionStatus(const bool value)
{
    setConfig().connection.proxyConnectionStatus = value;
    emit sig_changed();
}

int GeneralPageAdapter::getCharacterEncodingIndex() const
{
    return static_cast<int>(getConfig().general.characterEncoding);
}

void GeneralPageAdapter::setCharacterEncodingIndex(const int value)
{
    setConfig().general.characterEncoding = static_cast<CharacterEncodingEnum>(value);
    emit sig_changed();
}

int GeneralPageAdapter::getThemeIndex() const
{
    return static_cast<int>(getConfig().general.getTheme());
}

void GeneralPageAdapter::setThemeIndex(const int value)
{
    setConfig().general.setTheme(static_cast<ThemeEnum>(value));
    emit sig_changed();
}

bool GeneralPageAdapter::getEmulatedExits() const
{
    return getConfig().mumeNative.emulatedExits;
}

void GeneralPageAdapter::setEmulatedExits(const bool value)
{
    setConfig().mumeNative.emulatedExits = value;
    emit sig_changed();
}

bool GeneralPageAdapter::getShowHiddenExitFlags() const
{
    return getConfig().mumeNative.showHiddenExitFlags;
}

void GeneralPageAdapter::setShowHiddenExitFlags(const bool value)
{
    setConfig().mumeNative.showHiddenExitFlags = value;
    emit sig_changed();
}

bool GeneralPageAdapter::getShowNotes() const
{
    return getConfig().mumeNative.showNotes;
}

void GeneralPageAdapter::setShowNotes(const bool value)
{
    setConfig().mumeNative.showNotes = value;
    emit sig_changed();
}

bool GeneralPageAdapter::getCheckForUpdate() const
{
    return getConfig().general.checkForUpdate;
}

void GeneralPageAdapter::setCheckForUpdate(const bool value)
{
    setConfig().general.checkForUpdate = value;
    emit sig_changed();
}

bool GeneralPageAdapter::getUpdaterAvailable()
{
    return !NO_UPDATER;
}

bool GeneralPageAdapter::getQmlShell() const
{
    return getConfig().general.qmlShell;
}

void GeneralPageAdapter::setQmlShell(const bool value)
{
    setConfig().general.qmlShell = value;
    emit sig_changed();
}

QString GeneralPageAdapter::getAutoLoadFileName() const
{
    return getConfig().autoLoad.fileName;
}

void GeneralPageAdapter::setAutoLoadFileName(const QString &value)
{
    setConfig().autoLoad.fileName = value;
    emit sig_changed();
}

bool GeneralPageAdapter::getAutoLoadMap() const
{
    return getConfig().autoLoad.autoLoadMap;
}

void GeneralPageAdapter::setAutoLoadMap(const bool value)
{
    setConfig().autoLoad.autoLoadMap = value;
    emit sig_changed();
}

bool GeneralPageAdapter::getDisplayMumeClock() const
{
    return getConfig().mumeClock.display;
}

void GeneralPageAdapter::setDisplayMumeClock(const bool value)
{
    setConfig().mumeClock.display = value;
    emit sig_changed();
}

bool GeneralPageAdapter::getDisplayXPStatus() const
{
    return getConfig().adventurePanel.getDisplayXPStatus();
}

void GeneralPageAdapter::setDisplayXPStatus(const bool value)
{
    setConfig().adventurePanel.setDisplayXPStatus(value);
    emit sig_changed();
}

QString GeneralPageAdapter::getResourcesDirectory() const
{
    return getConfig().canvas.resourcesDirectory;
}

void GeneralPageAdapter::setResourcesDirectory(const QString &value)
{
    setConfig().canvas.resourcesDirectory = value;
    emit sig_changed();
}

bool GeneralPageAdapter::getRememberLogin() const
{
    return getConfig().account.rememberLogin;
}

void GeneralPageAdapter::setRememberLogin(const bool value)
{
    setConfig().account.rememberLogin = value;
    emit sig_changed();
}

bool GeneralPageAdapter::getKeychainAvailable()
{
    return !NO_QTKEYCHAIN;
}

bool GeneralPageAdapter::getAutoLoginEnabled() const
{
    if constexpr (NO_QTKEYCHAIN) {
        return false;
    }
    const auto &account = getConfig().account;
    return !account.accountName.isEmpty() && account.accountPassword;
}

bool GeneralPageAdapter::getHasPassword() const
{
    return getConfig().account.accountPassword;
}

void GeneralPageAdapter::selectWorldFile()
{
    const auto &savedLastMapDir = getConfig().autoLoad.lastMapDirectory;
    const QString fileName = QFileDialog::getOpenFileName(m_dialogParent,
                                                          tr("Choose map file ..."),
                                                          savedLastMapDir,
                                                          tr("MMapper2 (*.mm2);;MMapper (*.map)"));
    if (!fileName.isEmpty()) {
        auto &savedAutoLoad = setConfig().autoLoad;
        savedAutoLoad.fileName = fileName;
        savedAutoLoad.autoLoadMap = true;
        emit sig_changed();
    }
}

void GeneralPageAdapter::chooseResourcesDirectory()
{
    const auto &resourceDir = getConfig().canvas.resourcesDirectory;
    const QString directory = QFileDialog::getExistingDirectory(m_dialogParent,
                                                                tr("Choose resource location ..."),
                                                                resourceDir);
    if (!directory.isEmpty()) {
        setConfig().canvas.resourcesDirectory = directory;
        emit sig_changed();
    }
}

void GeneralPageAdapter::managePassword()
{
    const QString accountName = getConfig().account.accountName;

    if (getConfig().account.accountPassword) {
        m_passwordRequesting = true;
        m_passwordConfig.getPassword();
        return;
    }

    m_passwordDialogLauncher(
        accountName,
        QString(),
        /*hasStoredPassword=*/false,
        m_dialogParent,
        [this](const QString &newAccountName, const QString &password) {
            if (!password.isEmpty()) {
                setConfig().account.accountName = newAccountName;
                m_passwordConfig.setPassword(password);
            }
        },
        [this]() { m_passwordConfig.deletePassword(); });
}

void GeneralPageAdapter::factoryReset()
{
    auto *box = new QMessageBox(QMessageBox::Question,
                                tr("MMapper Factory Reset"),
                                tr("Are you sure you want to perform a factory reset?"),
                                QMessageBox::Yes | QMessageBox::No,
                                m_dialogParent);
    box->setAttribute(Qt::WA_DeleteOnClose);
    connect(box, &QMessageBox::finished, this, [this](int result) {
        if (result == QMessageBox::Yes) {
            setConfig().reset();
            emit sig_reloadConfig();
        }
    });
    box->open();
}

void GeneralPageAdapter::exportConfig()
{
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
}

void GeneralPageAdapter::importConfig()
{
    auto importFile = [this](const QString &fileName, const std::optional<QByteArray> &fileContent) {
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

    const auto nameFilter = tr("Configuration (*.ini);;All files (*)");
    QFileDialog::getOpenFileContent(nameFilter, importFile);
}

void GeneralPageAdapter::reload()
{
    emit sig_changed();
}
