#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "../global/macros.h"

#include <QObject>
#include <QRect>
#include <QString>
#include <QStringList>

// Owns the visibility (and, since the floating-dock commit, floating/
// docked) state of Shell B's 8 side-panel docks (see QmlShellWindow.cpp/
// MainShell.qml), analogous to the QDockWidget::hide()/show(),
// toggleViewAction() and setFloating() calls MainWindow makes directly on
// each QmlDockWidget in mainwindow.cpp. Under the single
// QQmlApplicationEngine the QML shell uses (unlike each QmlDockWidget's own
// private QQuickWidget engine), the docks are plain QQC2.SplitView children
// rather than QDockWidgets, so something has to own their visible/floating
// state instead -- that's this class. Exposed to QML as a single
// "dockLayout" context property (see QmlShellWindow.cpp); DockPanel.qml's
// close/float buttons and MainShell.qml's "Side Panels" menu items and
// per-dock SplitView/Loader bindings all read these properties.
//
// A flat list of 8 named bool properties per concern (rather than a
// QVariantMap/id-keyed API) was chosen because it lets QML bind each
// dock's `visible`/`floating` and each menu item's `checked` directly as a
// property expression (`dockLayout.logVisible`) instead of going through
// `dockLayout.isVisible("log")`-style invokables, which is both more
// verbose in the 8 call sites that need it and loses QML's automatic
// re-evaluation on change (Q_PROPERTY's NOTIFY signal drives that; a plain
// Q_INVOKABLE getter would not update automatically). The float-geometry
// QRect properties below follow the same flat-property shape (rather than a
// saveFloatGeometry(id,...)/floatGeometry(id) invokable pair) so
// QmlShellWindow.cpp's persistWindowState()/restoreWindowState() can read/
// write them with the exact same `m_dockLayout->property("xFloatGeometry")`
// pattern it already uses for the visibility flags below, instead of a
// second, invokable-based persistence code path.
//
// Deliberately NOT implemented here (left for a later "shell lifecycle"
// commit, see the task report): persisting dock area/index/size (only
// visibility, floating state and float geometry are covered).
// MainShell.qml's docked-dock areas/order/sizes are presently hard-coded to
// the same defaults mainwindow.cpp's addDockWidget() calls establish.
class NODISCARD_QOBJECT DockLayoutController final : public QObject
{
    Q_OBJECT

    // Defaults mirror mainwindow.cpp's dock construction: Log/Room/
    // Adventure/Timers/Tasks start hidden (dock->hide()); Group/Description/
    // Client start visible (no hide() call). Every dock starts docked (not
    // floating), matching QDockWidget's default DockWidgetFloatable-but-not-
    // floating state.
    Q_PROPERTY(bool logVisible MEMBER m_logVisible NOTIFY logVisibleChanged)
    Q_PROPERTY(bool groupVisible MEMBER m_groupVisible NOTIFY groupVisibleChanged)
    Q_PROPERTY(bool roomVisible MEMBER m_roomVisible NOTIFY roomVisibleChanged)
    Q_PROPERTY(bool adventureVisible MEMBER m_adventureVisible NOTIFY adventureVisibleChanged)
    Q_PROPERTY(bool descriptionVisible MEMBER m_descriptionVisible NOTIFY descriptionVisibleChanged)
    Q_PROPERTY(bool timersVisible MEMBER m_timersVisible NOTIFY timersVisibleChanged)
    Q_PROPERTY(bool tasksVisible MEMBER m_tasksVisible NOTIFY tasksVisibleChanged)
    Q_PROPERTY(bool clientVisible MEMBER m_clientVisible NOTIFY clientVisibleChanged)

    Q_PROPERTY(bool logFloating MEMBER m_logFloating NOTIFY logFloatingChanged)
    Q_PROPERTY(bool groupFloating MEMBER m_groupFloating NOTIFY groupFloatingChanged)
    Q_PROPERTY(bool roomFloating MEMBER m_roomFloating NOTIFY roomFloatingChanged)
    Q_PROPERTY(bool adventureFloating MEMBER m_adventureFloating NOTIFY adventureFloatingChanged)
    Q_PROPERTY(
        bool descriptionFloating MEMBER m_descriptionFloating NOTIFY descriptionFloatingChanged)
    Q_PROPERTY(bool timersFloating MEMBER m_timersFloating NOTIFY timersFloatingChanged)
    Q_PROPERTY(bool tasksFloating MEMBER m_tasksFloating NOTIFY tasksFloatingChanged)
    Q_PROPERTY(bool clientFloating MEMBER m_clientFloating NOTIFY clientFloatingChanged)

    // Last known geometry of each dock's floating QtQuick.Window (see
    // MainShell.qml's FloatingDock inline component), so re-floating a dock
    // (or restarting the app with it left floating -- see
    // Configuration::QmlShellSettings) restores where the user left it
    // instead of always reopening at Qt's default Window position/size. An
    // invalid (default-constructed, all-zero) QRect means "no saved
    // geometry yet" -- MainShell.qml only applies a saved geometry when
    // isValid() is true.
    Q_PROPERTY(QRect logFloatGeometry MEMBER m_logFloatGeometry NOTIFY logFloatGeometryChanged)
    Q_PROPERTY(QRect groupFloatGeometry MEMBER m_groupFloatGeometry NOTIFY groupFloatGeometryChanged)
    Q_PROPERTY(QRect roomFloatGeometry MEMBER m_roomFloatGeometry NOTIFY roomFloatGeometryChanged)
    Q_PROPERTY(QRect adventureFloatGeometry MEMBER m_adventureFloatGeometry NOTIFY
                   adventureFloatGeometryChanged)
    Q_PROPERTY(QRect descriptionFloatGeometry MEMBER m_descriptionFloatGeometry NOTIFY
                   descriptionFloatGeometryChanged)
    Q_PROPERTY(
        QRect timersFloatGeometry MEMBER m_timersFloatGeometry NOTIFY timersFloatGeometryChanged)
    Q_PROPERTY(QRect tasksFloatGeometry MEMBER m_tasksFloatGeometry NOTIFY tasksFloatGeometryChanged)
    Q_PROPERTY(
        QRect clientFloatGeometry MEMBER m_clientFloatGeometry NOTIFY clientFloatGeometryChanged)

    // Which of the 4 fixed SplitView slots (see MainShell.qml's leftColumn/
    // topRow/bottomRow/rightColumn) each dock currently belongs to, as one
    // of the literal strings "left"/"top"/"bottom"/"right". Unlike the
    // visible/floating/floatGeometry properties above, area is NOT exposed
    // per-dock as its own Q_PROPERTY (that would be 8 more flat properties
    // for something QML never needs to bind to directly) -- instead
    // MainShell.qml's DockPanel header "move to" menu calls the
    // setDockArea()/dockArea() invokables below, and the 4 lists below are
    // what MainShell.qml's per-area Repeaters actually bind against.
    //
    // The 4 lists are recomputed (recomputeAreaLists()) from the 8 areas
    // above plus the existing xVisible/xFloating state any time any of
    // those change, so a dock that's hidden or floating is automatically
    // absent from every area list -- MainShell.qml's Repeater-per-area
    // layout (see its "dock region" comment) relies on that to keep
    // "container visible == list non-empty" correct without re-deriving
    // visible-and-docked itself.
    Q_PROPERTY(QStringList leftDockIds MEMBER m_leftDockIds NOTIFY leftDockIdsChanged)
    Q_PROPERTY(QStringList topDockIds MEMBER m_topDockIds NOTIFY topDockIdsChanged)
    Q_PROPERTY(QStringList bottomDockIds MEMBER m_bottomDockIds NOTIFY bottomDockIdsChanged)
    Q_PROPERTY(QStringList rightDockIds MEMBER m_rightDockIds NOTIFY rightDockIdsChanged)

private:
    bool m_logVisible = false;
    bool m_groupVisible = true;
    bool m_roomVisible = false;
    bool m_adventureVisible = false;
    bool m_descriptionVisible = true;
    bool m_timersVisible = false;
    bool m_tasksVisible = false;
    bool m_clientVisible = true;

    bool m_logFloating = false;
    bool m_groupFloating = false;
    bool m_roomFloating = false;
    bool m_adventureFloating = false;
    bool m_descriptionFloating = false;
    bool m_timersFloating = false;
    bool m_tasksFloating = false;
    bool m_clientFloating = false;

    QRect m_logFloatGeometry;
    QRect m_groupFloatGeometry;
    QRect m_roomFloatGeometry;
    QRect m_adventureFloatGeometry;
    QRect m_descriptionFloatGeometry;
    QRect m_timersFloatGeometry;
    QRect m_tasksFloatGeometry;
    QRect m_clientFloatGeometry;

    // Defaults mirror MainShell.qml's previously-hard-coded SplitView
    // placement (see its "dock region" comment): Client left; Group top;
    // Log/Room/Adventure/Tasks bottom; Description/Timers right.
    QString m_logArea = QStringLiteral("bottom");
    QString m_groupArea = QStringLiteral("top");
    QString m_roomArea = QStringLiteral("bottom");
    QString m_adventureArea = QStringLiteral("bottom");
    QString m_descriptionArea = QStringLiteral("right");
    QString m_timersArea = QStringLiteral("right");
    QString m_tasksArea = QStringLiteral("bottom");
    QString m_clientArea = QStringLiteral("left");

    QStringList m_leftDockIds;
    QStringList m_topDockIds;
    QStringList m_bottomDockIds;
    QStringList m_rightDockIds;

public:
    explicit DockLayoutController(QObject *parent = nullptr);

    // Sets dockId's area (one of "left"/"top"/"bottom"/"right") and
    // recomputes the 4 xDockIds lists; both dockId and area are validated
    // against the fixed 8-id/4-area sets and silently ignored if either is
    // unrecognized (mirrors how an out-of-range property write would
    // normally just be a no-op rather than warrant a crash/assert here --
    // this is reachable directly from QML via MainShell.qml's "move to"
    // menu, which only ever passes the 8/4 literals it itself defines, but
    // defensive validation costs nothing and avoids ever storing a
    // nonsense area string that recomputeAreaLists() wouldn't know what to
    // do with).
    Q_INVOKABLE void setDockArea(const QString &id, const QString &area);

    // Returns dockId's current area, or an empty string for an
    // unrecognized id.
    Q_INVOKABLE QString dockArea(const QString &id) const;

private:
    // Rebuilds the 4 xDockIds lists from the current per-dock area/visible/
    // floating state, in the fixed canonical id order (client, group, log,
    // room, adventure, tasks, description, timers) -- see this class's file
    // comment -- and emits all 4 xDockIdsChanged signals. Connected to
    // every xVisibleChanged/xFloatingChanged signal in the constructor, and
    // also called directly by setDockArea(), so the lists always reflect
    // the latest state without callers having to remember to refresh them.
    void recomputeAreaLists();

signals:
    void logVisibleChanged();
    void groupVisibleChanged();
    void roomVisibleChanged();
    void adventureVisibleChanged();
    void descriptionVisibleChanged();
    void timersVisibleChanged();
    void tasksVisibleChanged();
    void clientVisibleChanged();

    void logFloatingChanged();
    void groupFloatingChanged();
    void roomFloatingChanged();
    void adventureFloatingChanged();
    void descriptionFloatingChanged();
    void timersFloatingChanged();
    void tasksFloatingChanged();
    void clientFloatingChanged();

    void logFloatGeometryChanged();
    void groupFloatGeometryChanged();
    void roomFloatGeometryChanged();
    void adventureFloatGeometryChanged();
    void descriptionFloatGeometryChanged();
    void timersFloatGeometryChanged();
    void tasksFloatGeometryChanged();
    void clientFloatGeometryChanged();

    void leftDockIdsChanged();
    void topDockIdsChanged();
    void bottomDockIdsChanged();
    void rightDockIdsChanged();
};
