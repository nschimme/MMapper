// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "GeneralViewModel.h"
#include "../configuration/configuration.h"

GeneralViewModel::GeneralViewModel(QObject *p) : QObject(p) {}

#define CFG_GET(Type, Name, Field) \
Type GeneralViewModel::Name() const { return Field; }

#define CFG_SET(Type, Setter, Field) \
void GeneralViewModel::Setter(Type v) { if (Field != v) { Field = v; emit settingsChanged(); } }

#define CFG_GETSET(Type, Name, Setter, Field) \
CFG_GET(Type, Name, Field) \
CFG_SET(Type, Setter, Field)

CFG_GETSET(bool, tlsEncryption, setTlsEncryption, setConfig().connection.tlsEncryption)
CFG_GETSET(bool, proxyListensOnAnyInterface, setProxyListensOnAnyInterface, setConfig().connection.proxyListensOnAnyInterface)
CFG_GETSET(bool, emulateExits, setEmulateExits, setConfig().mumeNative.emulatedExits)
CFG_GETSET(bool, showHiddenExitFlags, setShowHiddenExitFlags, setConfig().mumeNative.showHiddenExitFlags)
CFG_GETSET(bool, showNotes, setShowNotes, setConfig().mumeNative.showNotes)
CFG_GETSET(bool, checkForUpdate, setCheckForUpdate, setConfig().general.checkForUpdate)
CFG_GETSET(bool, autoLoadMap, setAutoLoadMap, setConfig().autoLoad.autoLoadMap)
CFG_GETSET(bool, displayMumeClock, setDisplayMumeClock, setConfig().mumeClock.display)
CFG_GETSET(bool, proxyConnectionStatus, setProxyConnectionStatus, setConfig().connection.proxyConnectionStatus)
CFG_GETSET(bool, rememberLogin, setRememberLogin, setConfig().account.rememberLogin)

QString GeneralViewModel::remoteName() const { return setConfig().connection.remoteServerName; }
void GeneralViewModel::setRemoteName(const QString &v) { if (setConfig().connection.remoteServerName != v) { setConfig().connection.remoteServerName = v; emit settingsChanged(); } }
QString GeneralViewModel::autoLoadFileName() const { return setConfig().autoLoad.fileName; }
void GeneralViewModel::setAutoLoadFileName(const QString &v) { if (setConfig().autoLoad.fileName != v) { setConfig().autoLoad.fileName = v; emit settingsChanged(); } }
QString GeneralViewModel::accountName() const { return setConfig().account.accountName; }
void GeneralViewModel::setAccountName(const QString &v) { if (setConfig().account.accountName != v) { setConfig().account.accountName = v; emit settingsChanged(); } }

int GeneralViewModel::remotePort() const { return setConfig().connection.remotePort; }
void GeneralViewModel::setRemotePort(int v) { if (setConfig().connection.remotePort != v) { setConfig().connection.remotePort = static_cast<uint16_t>(v); emit settingsChanged(); } }
int GeneralViewModel::localPort() const { return setConfig().connection.localPort; }
void GeneralViewModel::setLocalPort(int v) { if (setConfig().connection.localPort != v) { setConfig().connection.localPort = static_cast<uint16_t>(v); emit settingsChanged(); } }
int GeneralViewModel::characterEncoding() const { return static_cast<int>(setConfig().general.characterEncoding); }
void GeneralViewModel::setCharacterEncoding(int v) { if (static_cast<int>(setConfig().general.characterEncoding) != v) { setConfig().general.characterEncoding = static_cast<CharacterEncodingEnum>(v); emit settingsChanged(); } }

int GeneralViewModel::theme() const { return static_cast<int>(getConfig().general.getTheme()); }
void GeneralViewModel::setTheme(int v) { if (static_cast<int>(getConfig().general.getTheme()) != v) { setConfig().general.setTheme(static_cast<ThemeEnum>(v)); emit settingsChanged(); } }
bool GeneralViewModel::displayXPStatus() const { return getConfig().adventurePanel.getDisplayXPStatus(); }
void GeneralViewModel::setDisplayXPStatus(bool v) { if (getConfig().adventurePanel.getDisplayXPStatus() != v) { setConfig().adventurePanel.setDisplayXPStatus(v); emit settingsChanged(); } }

void GeneralViewModel::loadConfig() { emit settingsChanged(); }
void GeneralViewModel::resetConfig() { setConfig().reset(); emit sig_reloadConfig(); emit settingsChanged(); }
