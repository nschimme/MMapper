// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "AutoLogPageAdapter.h"

#include "../configuration/configuration.h"
#include "../global/ConfigEnums.h"

#include <QFileDialog>

namespace {
constexpr const int MEGABYTE_IN_BYTES = 1000000;
} // namespace

AutoLogPageAdapter::AutoLogPageAdapter(QWidget *const dialogParent, QObject *const parent)
    : QObject(parent)
    , m_dialogParent(dialogParent)
{}

bool AutoLogPageAdapter::getAutoLog() const
{
    return getConfig().autoLog.autoLog;
}

void AutoLogPageAdapter::setAutoLog(const bool value)
{
    setConfig().autoLog.autoLog = value;
    emit sig_changed();
}

QString AutoLogPageAdapter::getAutoLogDirectory() const
{
    return getConfig().autoLog.autoLogDirectory;
}

void AutoLogPageAdapter::setAutoLogDirectory(const QString &value)
{
    setConfig().autoLog.autoLogDirectory = value;
    emit sig_changed();
}

int AutoLogPageAdapter::getRotateWhenLogsReachMB() const
{
    return getConfig().autoLog.rotateWhenLogsReachBytes / MEGABYTE_IN_BYTES;
}

void AutoLogPageAdapter::setRotateWhenLogsReachMB(const int value)
{
    setConfig().autoLog.rotateWhenLogsReachBytes = value * MEGABYTE_IN_BYTES;
    emit sig_changed();
}

int AutoLogPageAdapter::getCleanupStrategy() const
{
    return static_cast<int>(getConfig().autoLog.cleanupStrategy);
}

void AutoLogPageAdapter::setCleanupStrategy(const int value)
{
    setConfig().autoLog.cleanupStrategy = static_cast<AutoLoggerEnum>(value);
    emit sig_changed();
}

int AutoLogPageAdapter::getDeleteWhenLogsReachDays() const
{
    return getConfig().autoLog.deleteWhenLogsReachDays;
}

void AutoLogPageAdapter::setDeleteWhenLogsReachDays(const int value)
{
    setConfig().autoLog.deleteWhenLogsReachDays = value;
    emit sig_changed();
}

int AutoLogPageAdapter::getDeleteWhenLogsReachMB() const
{
    return getConfig().autoLog.deleteWhenLogsReachBytes / MEGABYTE_IN_BYTES;
}

void AutoLogPageAdapter::setDeleteWhenLogsReachMB(const int value)
{
    setConfig().autoLog.deleteWhenLogsReachBytes = value * MEGABYTE_IN_BYTES;
    emit sig_changed();
}

bool AutoLogPageAdapter::getAskDelete() const
{
    return getConfig().autoLog.askDelete;
}

void AutoLogPageAdapter::setAskDelete(const bool value)
{
    setConfig().autoLog.askDelete = value;
    emit sig_changed();
}

bool AutoLogPageAdapter::getIsWasm()
{
    return CURRENT_PLATFORM == PlatformEnum::Wasm;
}

void AutoLogPageAdapter::browseForDirectory()
{
    auto &config = setConfig().autoLog;
    const QString logDirectory = QFileDialog::getExistingDirectory(m_dialogParent,
                                                                   tr("Choose log location ..."),
                                                                   config.autoLogDirectory);
    if (!logDirectory.isEmpty()) {
        config.autoLogDirectory = logDirectory;
        emit sig_changed();
    }
}

void AutoLogPageAdapter::reload()
{
    emit sig_changed();
}
