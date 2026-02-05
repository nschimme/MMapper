// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#include "AutoLogPageViewModel.h"
#include "../configuration/configuration.h"

AutoLogPageViewModel::AutoLogPageViewModel(QObject *p) : QObject(p) {}

void AutoLogPageViewModel::setAutoLogDirectory(const QString &v) { if (setConfig().autoLog.autoLogDirectory != v) { setConfig().autoLog.autoLogDirectory = v; emit settingsChanged(); } }
QString AutoLogPageViewModel::autoLogDirectory() const { return getConfig().autoLog.autoLogDirectory; }

void AutoLogPageViewModel::setAutoLog(bool v) { if (setConfig().autoLog.autoLog != v) { setConfig().autoLog.autoLog = v; emit settingsChanged(); } }
bool AutoLogPageViewModel::autoLog() const { return getConfig().autoLog.autoLog; }

void AutoLogPageViewModel::setAskDelete(bool v) { if (setConfig().autoLog.askDelete != v) { setConfig().autoLog.askDelete = v; emit settingsChanged(); } }
bool AutoLogPageViewModel::askDelete() const { return getConfig().autoLog.askDelete; }

int AutoLogPageViewModel::cleanupStrategy() const { return static_cast<int>(getConfig().autoLog.cleanupStrategy); }
void AutoLogPageViewModel::setCleanupStrategy(int v) { if (static_cast<int>(getConfig().autoLog.cleanupStrategy) != v) { setConfig().autoLog.cleanupStrategy = static_cast<AutoLoggerEnum>(v); emit settingsChanged(); } }
int AutoLogPageViewModel::deleteWhenLogsReachDays() const { return getConfig().autoLog.deleteWhenLogsReachDays; }
void AutoLogPageViewModel::setDeleteWhenLogsReachDays(int v) { if (getConfig().autoLog.deleteWhenLogsReachDays != v) { setConfig().autoLog.deleteWhenLogsReachDays = v; emit settingsChanged(); } }
int AutoLogPageViewModel::deleteWhenLogsReachBytes() const { return getConfig().autoLog.deleteWhenLogsReachBytes; }
void AutoLogPageViewModel::setDeleteWhenLogsReachBytes(int v) { if (getConfig().autoLog.deleteWhenLogsReachBytes != v) { setConfig().autoLog.deleteWhenLogsReachBytes = v; emit settingsChanged(); } }
int AutoLogPageViewModel::rotateWhenLogsReachBytes() const { return getConfig().autoLog.rotateWhenLogsReachBytes; }
void AutoLogPageViewModel::setRotateWhenLogsReachBytes(int v) { if (getConfig().autoLog.rotateWhenLogsReachBytes != v) { setConfig().autoLog.rotateWhenLogsReachBytes = v; emit settingsChanged(); } }

void AutoLogPageViewModel::loadConfig() { emit settingsChanged(); }
