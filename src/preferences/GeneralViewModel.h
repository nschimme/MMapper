#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#include "../global/macros.h"
#include <QObject>
#include <QString>

class NODISCARD_QOBJECT GeneralViewModel final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString remoteName READ remoteName WRITE setRemoteName NOTIFY settingsChanged)
    Q_PROPERTY(int remotePort READ remotePort WRITE setRemotePort NOTIFY settingsChanged)
    Q_PROPERTY(int localPort READ localPort WRITE setLocalPort NOTIFY settingsChanged)
    Q_PROPERTY(bool tlsEncryption READ tlsEncryption WRITE setTlsEncryption NOTIFY settingsChanged)
    Q_PROPERTY(bool proxyListensOnAnyInterface READ proxyListensOnAnyInterface WRITE setProxyListensOnAnyInterface NOTIFY settingsChanged)
    Q_PROPERTY(int characterEncoding READ characterEncoding WRITE setCharacterEncoding NOTIFY settingsChanged)
    Q_PROPERTY(int theme READ theme WRITE setTheme NOTIFY settingsChanged)
    Q_PROPERTY(bool emulateExits READ emulateExits WRITE setEmulateExits NOTIFY settingsChanged)
    Q_PROPERTY(bool showHiddenExitFlags READ showHiddenExitFlags WRITE setShowHiddenExitFlags NOTIFY settingsChanged)
    Q_PROPERTY(bool showNotes READ showNotes WRITE setShowNotes NOTIFY settingsChanged)
    Q_PROPERTY(bool checkForUpdate READ checkForUpdate WRITE setCheckForUpdate NOTIFY settingsChanged)
    Q_PROPERTY(QString autoLoadFileName READ autoLoadFileName WRITE setAutoLoadFileName NOTIFY settingsChanged)
    Q_PROPERTY(bool autoLoadMap READ autoLoadMap WRITE setAutoLoadMap NOTIFY settingsChanged)
    Q_PROPERTY(bool displayMumeClock READ displayMumeClock WRITE setDisplayMumeClock NOTIFY settingsChanged)
    Q_PROPERTY(bool displayXPStatus READ displayXPStatus WRITE setDisplayXPStatus NOTIFY settingsChanged)
    Q_PROPERTY(bool proxyConnectionStatus READ proxyConnectionStatus WRITE setProxyConnectionStatus NOTIFY settingsChanged)
    Q_PROPERTY(bool rememberLogin READ rememberLogin WRITE setRememberLogin NOTIFY settingsChanged)
    Q_PROPERTY(QString accountName READ accountName WRITE setAccountName NOTIFY settingsChanged)

public:
    explicit GeneralViewModel(QObject *parent = nullptr);

    NODISCARD QString remoteName() const; void setRemoteName(const QString &v);
    NODISCARD int remotePort() const; void setRemotePort(int v);
    NODISCARD int localPort() const; void setLocalPort(int v);
    NODISCARD bool tlsEncryption() const; void setTlsEncryption(bool v);
    NODISCARD bool proxyListensOnAnyInterface() const; void setProxyListensOnAnyInterface(bool v);
    NODISCARD int characterEncoding() const; void setCharacterEncoding(int v);
    NODISCARD int theme() const; void setTheme(int v);
    NODISCARD bool emulateExits() const; void setEmulateExits(bool v);
    NODISCARD bool showHiddenExitFlags() const; void setShowHiddenExitFlags(bool v);
    NODISCARD bool showNotes() const; void setShowNotes(bool v);
    NODISCARD bool checkForUpdate() const; void setCheckForUpdate(bool v);
    NODISCARD QString autoLoadFileName() const; void setAutoLoadFileName(const QString &v);
    NODISCARD bool autoLoadMap() const; void setAutoLoadMap(bool v);
    NODISCARD bool displayMumeClock() const; void setDisplayMumeClock(bool v);
    NODISCARD bool displayXPStatus() const; void setDisplayXPStatus(bool v);
    NODISCARD bool proxyConnectionStatus() const; void setProxyConnectionStatus(bool v);
    NODISCARD bool rememberLogin() const; void setRememberLogin(bool v);
    NODISCARD QString accountName() const; void setAccountName(const QString &v);

    void loadConfig();
    void resetConfig();

signals:
    void settingsChanged();
    void sig_reloadConfig();
};
