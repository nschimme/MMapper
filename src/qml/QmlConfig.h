#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "../global/macros.h"

#include <QColor>
#include <QObject>

// Q_PROPERTY façade over the plain-struct GroupManagerSettings subgroup of
// Configuration (see ../configuration/configuration.h). Unlike most other
// config subgroups, GroupManagerSettings has no ChangeMonitor: it is a bag
// of public fields written directly by callers (most notably the
// preferences dialog). That means this façade cannot passively observe
// changes; each setter here notifies QML itself, but any code path that
// writes setConfig().groupManager.* directly (outside of this class'
// setters) will leave QML holding stale values until reload() is called
// explicitly. MainWindow is responsible for calling reload() after any
// dialog that may have touched these fields (e.g. ConfigDialog) closes or
// signals a change. Other external mutations not funneled through such a
// hook are accepted as stale until the next reload().
class NODISCARD_QOBJECT QmlConfig final : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool npcHide READ getNpcHide WRITE setNpcHide NOTIFY npcHideChanged)
    Q_PROPERTY(
        bool npcSortBottom READ getNpcSortBottom WRITE setNpcSortBottom NOTIFY npcSortBottomChanged)
    Q_PROPERTY(QColor groupColor READ getGroupColor WRITE setGroupColor NOTIFY groupColorChanged)

private:
    // Cache of the last-known values, used only to detect external changes
    // in reload(); the getters below always read the live Configuration.
    bool m_npcHide = false;
    bool m_npcSortBottom = false;
    QColor m_groupColor;

public:
    explicit QmlConfig(QObject *parent = nullptr);

public:
    NODISCARD bool getNpcHide() const;
    void setNpcHide(bool value);

    NODISCARD bool getNpcSortBottom() const;
    void setNpcSortBottom(bool value);

    NODISCARD QColor getGroupColor() const;
    void setGroupColor(const QColor &value);

public slots:
    // Re-syncs the cached values against the live Configuration and emits
    // the appropriate *Changed signals for anything that differs. Must be
    // called whenever code outside this class may have written to
    // getConfig().groupManager.
    void reload();

signals:
    void npcHideChanged();
    void npcSortBottomChanged();
    void groupColorChanged();
};
