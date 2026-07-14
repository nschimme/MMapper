#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "../configuration/PasswordConfig.h"
#include "../global/macros.h"

#include <QObject>
#include <QPointer>
#include <QString>

class QWidget;

// Q_PROPERTY façade over several Configuration groups (connection, general,
// mumeNative, autoLoad, mumeClock, adventurePanel, account, canvas -- see
// ../configuration/configuration.h), mirroring generalpage.cpp's
// apply-on-change semantics. None of the touched groups have a
// ChangeMonitor, so reload() re-emits sig_changed unconditionally.
//
// characterEncodingIndex/themeIndex are plain 0-based indices into
// charsetComboBox/themeComboBox's fixed drop-down order (see the
// static_asserts at the top of generalpage.cpp, mirrored here); QML owns the
// label list.
//
// managePassword()/factoryReset()/exportConfig()/importConfig() keep every
// native dialog (QMessageBox confirmations, QFileDialog content APIs, and
// the keychain round-trip via PasswordConfig + ManagePasswordDialog)
// exactly as generalpage.cpp implements them; see managePassword()'s
// definition in GeneralPageAdapter.cpp for why this stays off QML entirely.
// factoryReset()/importConfig() emit sig_reloadConfig(), which
// PreferencesController forwards into reloadAll(), mirroring
// GeneralPage::sig_reloadConfig() -> ConfigDialog::slot_loadConfig().
class NODISCARD_QOBJECT GeneralPageAdapter final : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString remoteName READ getRemoteName WRITE setRemoteName NOTIFY sig_changed)
    Q_PROPERTY(int remotePort READ getRemotePort WRITE setRemotePort NOTIFY sig_changed)
    Q_PROPERTY(int localPort READ getLocalPort WRITE setLocalPort NOTIFY sig_changed)
    Q_PROPERTY(bool tlsEncryption READ getTlsEncryption WRITE setTlsEncryption NOTIFY sig_changed)
    Q_PROPERTY(bool tlsAvailable READ getTlsAvailable CONSTANT)
    Q_PROPERTY(bool proxyListensOnAnyInterface READ getProxyListensOnAnyInterface WRITE
                   setProxyListensOnAnyInterface NOTIFY sig_changed)
    Q_PROPERTY(bool proxyConnectionStatus READ getProxyConnectionStatus WRITE
                   setProxyConnectionStatus NOTIFY sig_changed)

    Q_PROPERTY(int characterEncodingIndex READ getCharacterEncodingIndex WRITE
                   setCharacterEncodingIndex NOTIFY sig_changed)
    Q_PROPERTY(int themeIndex READ getThemeIndex WRITE setThemeIndex NOTIFY sig_changed)

    Q_PROPERTY(bool emulatedExits READ getEmulatedExits WRITE setEmulatedExits NOTIFY sig_changed)
    Q_PROPERTY(bool showHiddenExitFlags READ getShowHiddenExitFlags WRITE setShowHiddenExitFlags
                   NOTIFY sig_changed)
    Q_PROPERTY(bool showNotes READ getShowNotes WRITE setShowNotes NOTIFY sig_changed)

    Q_PROPERTY(bool checkForUpdate READ getCheckForUpdate WRITE setCheckForUpdate NOTIFY sig_changed)
    Q_PROPERTY(bool updaterAvailable READ getUpdaterAvailable CONSTANT)

    Q_PROPERTY(QString autoLoadFileName READ getAutoLoadFileName WRITE setAutoLoadFileName NOTIFY
                   sig_changed)
    Q_PROPERTY(bool autoLoadMap READ getAutoLoadMap WRITE setAutoLoadMap NOTIFY sig_changed)

    Q_PROPERTY(
        bool displayMumeClock READ getDisplayMumeClock WRITE setDisplayMumeClock NOTIFY sig_changed)
    Q_PROPERTY(
        bool displayXPStatus READ getDisplayXPStatus WRITE setDisplayXPStatus NOTIFY sig_changed)

    Q_PROPERTY(QString resourcesDirectory READ getResourcesDirectory WRITE setResourcesDirectory
                   NOTIFY sig_changed)

    Q_PROPERTY(bool rememberLogin READ getRememberLogin WRITE setRememberLogin NOTIFY sig_changed)
    Q_PROPERTY(bool keychainAvailable READ getKeychainAvailable CONSTANT)
    Q_PROPERTY(bool autoLoginEnabled READ getAutoLoginEnabled NOTIFY sig_changed)
    Q_PROPERTY(bool hasPassword READ getHasPassword NOTIFY sig_changed)

private:
    // Parent for every native dialog invoked below (QFileDialog,
    // QMessageBox, ManagePasswordDialog); owned by PreferencesController,
    // not by this adapter.
    QPointer<QWidget> m_dialogParent;
    PasswordConfig m_passwordConfig;
    bool m_passwordRequesting = false;

public:
    explicit GeneralPageAdapter(QWidget *dialogParent, QObject *parent);

public:
    NODISCARD QString getRemoteName() const;
    void setRemoteName(const QString &value);
    NODISCARD int getRemotePort() const;
    void setRemotePort(int value);
    NODISCARD int getLocalPort() const;
    void setLocalPort(int value);
    NODISCARD bool getTlsEncryption() const;
    void setTlsEncryption(bool value);
    NODISCARD static bool getTlsAvailable();
    NODISCARD bool getProxyListensOnAnyInterface() const;
    void setProxyListensOnAnyInterface(bool value);
    NODISCARD bool getProxyConnectionStatus() const;
    void setProxyConnectionStatus(bool value);

    NODISCARD int getCharacterEncodingIndex() const;
    void setCharacterEncodingIndex(int value);
    NODISCARD int getThemeIndex() const;
    void setThemeIndex(int value);

    NODISCARD bool getEmulatedExits() const;
    void setEmulatedExits(bool value);
    NODISCARD bool getShowHiddenExitFlags() const;
    void setShowHiddenExitFlags(bool value);
    NODISCARD bool getShowNotes() const;
    void setShowNotes(bool value);

    NODISCARD bool getCheckForUpdate() const;
    void setCheckForUpdate(bool value);
    NODISCARD static bool getUpdaterAvailable();

    NODISCARD QString getAutoLoadFileName() const;
    void setAutoLoadFileName(const QString &value);
    NODISCARD bool getAutoLoadMap() const;
    void setAutoLoadMap(bool value);

    NODISCARD bool getDisplayMumeClock() const;
    void setDisplayMumeClock(bool value);
    NODISCARD bool getDisplayXPStatus() const;
    void setDisplayXPStatus(bool value);

    NODISCARD QString getResourcesDirectory() const;
    void setResourcesDirectory(const QString &value);

    NODISCARD bool getRememberLogin() const;
    void setRememberLogin(bool value);
    NODISCARD static bool getKeychainAvailable();
    NODISCARD bool getAutoLoginEnabled() const;
    NODISCARD bool getHasPassword() const;

public:
    // Native QFileDialog picker, mirroring
    // GeneralPage::slot_selectWorldFileButtonClicked().
    Q_INVOKABLE void selectWorldFile();
    // Native QFileDialog picker, mirroring the resourcePushButton handler.
    Q_INVOKABLE void chooseResourcesDirectory();
    // Native keychain round-trip + ManagePasswordDialog, mirroring
    // GeneralPage::slot_setPasswordClicked() and its PasswordConfig signal
    // handlers.
    Q_INVOKABLE void managePassword();
    // Native confirmation QMessageBox, mirroring configurationResetButton's
    // handler.
    Q_INVOKABLE void factoryReset();
    // Native QFileDialog content APIs, mirroring
    // configurationExportButton/configurationImportButton's handlers.
    Q_INVOKABLE void exportConfig();
    Q_INVOKABLE void importConfig();

public slots:
    void reload();

signals:
    void sig_changed();
    // Forwarded by PreferencesController into reloadAll(); mirrors
    // GeneralPage::sig_reloadConfig().
    void sig_reloadConfig();
};
