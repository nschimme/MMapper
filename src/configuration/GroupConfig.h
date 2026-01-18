#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "../global/ChangeMonitor.h"
#include "../global/RuleOf5.h"
#include "../global/macros.h"

#include <functional>

#include <QString>

class QSettings;

class NODISCARD GroupConfig final
{
public:
    using ReadCallback = std::function<void(const QSettings &)>;
    using WriteCallback = std::function<void(QSettings &)>;

private:
    QString m_groupName;
    ReadCallback m_readCallback;
    WriteCallback m_writeCallback;
    ChangeMonitor m_changeMonitor;

public:
    GroupConfig() = delete;
    DELETE_CTORS_AND_ASSIGN_OPS(GroupConfig);

    explicit GroupConfig(QString groupName);

    void registerCallbacks(ReadCallback readCallback, WriteCallback writeCallback);

    void read(const QSettings &settings);
    void write(QSettings &settings) const;

    NODISCARD const QString &getName() const { return m_groupName; }

    void notifyChanged();
    void registerChangeCallback(const ChangeMonitor::Lifetime &lifetime,
                                ChangeMonitor::Function callback);
};
