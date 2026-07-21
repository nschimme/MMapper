// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "DockLayoutController.h"

#include <array>

namespace {

// Canonical order of the 8 dock ids -- see MainShell.qml's "canonical
// order" comment for why this specific order matters (it's what the task's
// tests assert the area lists come back in).
constexpr std::array<const char *, 8>
    DOCK_IDS{"client", "group", "log", "room", "adventure", "tasks", "description", "timers"};

NODISCARD bool isValidArea(const QString &area)
{
    return area == QStringLiteral("left") || area == QStringLiteral("top")
           || area == QStringLiteral("bottom") || area == QStringLiteral("right");
}

NODISCARD bool isValidId(const QString &id)
{
    for (const char *const dockId : DOCK_IDS) {
        if (id == QLatin1String(dockId)) {
            return true;
        }
    }
    return false;
}

} // namespace

DockLayoutController::DockLayoutController(QObject *const parent)
    : QObject(parent)
{
    // MainShell.qml's per-area Repeaters (see its "dock region" comment)
    // must drop a dock from its list the instant it's hidden or floated
    // away, exactly like the old per-container `visible` boolean-OR
    // expressions this replaces did -- wiring these two signals per dock is
    // what keeps that true without every call site that flips xVisible/
    // xFloating also having to remember to call recomputeAreaLists()
    // itself.
    connect(this,
            &DockLayoutController::logVisibleChanged,
            this,
            &DockLayoutController::recomputeAreaLists);
    connect(this,
            &DockLayoutController::groupVisibleChanged,
            this,
            &DockLayoutController::recomputeAreaLists);
    connect(this,
            &DockLayoutController::roomVisibleChanged,
            this,
            &DockLayoutController::recomputeAreaLists);
    connect(this,
            &DockLayoutController::adventureVisibleChanged,
            this,
            &DockLayoutController::recomputeAreaLists);
    connect(this,
            &DockLayoutController::descriptionVisibleChanged,
            this,
            &DockLayoutController::recomputeAreaLists);
    connect(this,
            &DockLayoutController::timersVisibleChanged,
            this,
            &DockLayoutController::recomputeAreaLists);
    connect(this,
            &DockLayoutController::tasksVisibleChanged,
            this,
            &DockLayoutController::recomputeAreaLists);
    connect(this,
            &DockLayoutController::clientVisibleChanged,
            this,
            &DockLayoutController::recomputeAreaLists);

    connect(this,
            &DockLayoutController::logFloatingChanged,
            this,
            &DockLayoutController::recomputeAreaLists);
    connect(this,
            &DockLayoutController::groupFloatingChanged,
            this,
            &DockLayoutController::recomputeAreaLists);
    connect(this,
            &DockLayoutController::roomFloatingChanged,
            this,
            &DockLayoutController::recomputeAreaLists);
    connect(this,
            &DockLayoutController::adventureFloatingChanged,
            this,
            &DockLayoutController::recomputeAreaLists);
    connect(this,
            &DockLayoutController::descriptionFloatingChanged,
            this,
            &DockLayoutController::recomputeAreaLists);
    connect(this,
            &DockLayoutController::timersFloatingChanged,
            this,
            &DockLayoutController::recomputeAreaLists);
    connect(this,
            &DockLayoutController::tasksFloatingChanged,
            this,
            &DockLayoutController::recomputeAreaLists);
    connect(this,
            &DockLayoutController::clientFloatingChanged,
            this,
            &DockLayoutController::recomputeAreaLists);

    recomputeAreaLists();
}

void DockLayoutController::setDockArea(const QString &id, const QString &area)
{
    if (!isValidArea(area) || !isValidId(id)) {
        return;
    }
    if (id == QStringLiteral("client")) {
        m_clientArea = area;
    } else if (id == QStringLiteral("group")) {
        m_groupArea = area;
    } else if (id == QStringLiteral("log")) {
        m_logArea = area;
    } else if (id == QStringLiteral("room")) {
        m_roomArea = area;
    } else if (id == QStringLiteral("adventure")) {
        m_adventureArea = area;
    } else if (id == QStringLiteral("tasks")) {
        m_tasksArea = area;
    } else if (id == QStringLiteral("description")) {
        m_descriptionArea = area;
    } else if (id == QStringLiteral("timers")) {
        m_timersArea = area;
    } else {
        return;
    }
    recomputeAreaLists();
}

QString DockLayoutController::dockArea(const QString &id) const
{
    if (id == QStringLiteral("client")) {
        return m_clientArea;
    } else if (id == QStringLiteral("group")) {
        return m_groupArea;
    } else if (id == QStringLiteral("log")) {
        return m_logArea;
    } else if (id == QStringLiteral("room")) {
        return m_roomArea;
    } else if (id == QStringLiteral("adventure")) {
        return m_adventureArea;
    } else if (id == QStringLiteral("tasks")) {
        return m_tasksArea;
    } else if (id == QStringLiteral("description")) {
        return m_descriptionArea;
    } else if (id == QStringLiteral("timers")) {
        return m_timersArea;
    }
    return QString();
}

void DockLayoutController::recomputeAreaLists()
{
    QStringList left;
    QStringList top;
    QStringList bottom;
    QStringList right;

    // Canonical id order -- see DOCK_IDS's comment above.
    struct NODISCARD Snapshot final
    {
        const char *id;
        QString area;
        bool visible;
        bool floating;
    };
    const std::array<Snapshot, 8> snapshots{{
        {"client", m_clientArea, m_clientVisible, m_clientFloating},
        {"group", m_groupArea, m_groupVisible, m_groupFloating},
        {"log", m_logArea, m_logVisible, m_logFloating},
        {"room", m_roomArea, m_roomVisible, m_roomFloating},
        {"adventure", m_adventureArea, m_adventureVisible, m_adventureFloating},
        {"tasks", m_tasksArea, m_tasksVisible, m_tasksFloating},
        {"description", m_descriptionArea, m_descriptionVisible, m_descriptionFloating},
        {"timers", m_timersArea, m_timersVisible, m_timersFloating},
    }};

    for (const auto &snapshot : snapshots) {
        if (!snapshot.visible || snapshot.floating) {
            continue;
        }
        const QString id = QString::fromLatin1(snapshot.id);
        if (snapshot.area == QStringLiteral("left")) {
            left.append(id);
        } else if (snapshot.area == QStringLiteral("top")) {
            top.append(id);
        } else if (snapshot.area == QStringLiteral("bottom")) {
            bottom.append(id);
        } else if (snapshot.area == QStringLiteral("right")) {
            right.append(id);
        }
    }

    m_leftDockIds = left;
    m_topDockIds = top;
    m_bottomDockIds = bottom;
    m_rightDockIds = right;

    emit leftDockIdsChanged();
    emit topDockIdsChanged();
    emit bottomDockIdsChanged();
    emit rightDockIdsChanged();
}
