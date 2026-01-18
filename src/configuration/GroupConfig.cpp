// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "GroupConfig.h"

#include <QSettings>

GroupConfig::GroupConfig(QString groupName)
    : m_groupName(std::move(groupName))
{}

void GroupConfig::registerCallbacks(ReadCallback readCallback, WriteCallback writeCallback)
{
    m_readCallback = std::move(readCallback);
    m_writeCallback = std::move(writeCallback);
}

void GroupConfig::read(const QSettings &settings)
{
    if (m_readCallback) {
        m_readCallback(settings);
    }
}

void GroupConfig::write(QSettings &settings) const
{
    if (m_writeCallback) {
        m_writeCallback(settings);
    }
}

void GroupConfig::notifyChanged()
{
    m_changeMonitor.notifyAll();
}

void GroupConfig::registerChangeCallback(const ChangeMonitor::Lifetime &lifetime,
                                         ChangeMonitor::Function callback)
{
    m_changeMonitor.registerChangeCallback(lifetime, std::move(callback));
}
