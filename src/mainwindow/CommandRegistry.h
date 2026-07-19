#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "../global/RuleOf5.h"
#include "../global/macros.h"
#include "UiCommand.h"

#include <QList>
#include <QMap>
#include <QObject>
#include <QString>
#include <QtCore>

class QAction;

// CommandRegistry owns every UiCommand MainWindow creates (one per QAction;
// see MainWindow::createActions()) and is the single source of truth the
// upcoming QML shell will bind against instead of QActions.
//
// Id scheme: dot-separated, kebab-case segments, "<namespace>.<action>",
// e.g. "file.new", "mouse-mode.move", "room.edit-selected". The namespaces
// in use (see MainWindow::createActions() for the authoritative full list):
//   file          - new/open/merge/reload/save/save-as/export.*/exit
//   edit          - undo/redo/preferences
//   view          - zoom-in/zoom-out/zoom-reset/always-on-top/
//                   show-status-bar/show-scroll-bars/show-menu-bar
//   layer         - up/down/reset
//   mouse-mode    - the 9 exclusive canvas mouse modes
//   mapper-mode   - the 3 exclusive play/map/offline modes
//   room          - create/edit-selected/delete-selected/move-*-selected/
//                   merge-*-selected/connect-to-neighbours/find/
//                   goto-selected/force-update-selected
//   connection    - delete-selected
//   infomark      - edit-selected/delete-selected
//   client        - launch/save-log/save-log-html
//   pathmachine   - release-all-paths
//   world         - rebuild-meshes
//   help          - vote/check-for-update/website/forum/wiki/setup/newbie/
//                   report-issue/about/about-qt
class NODISCARD_QOBJECT CommandRegistry final : public QObject
{
    Q_OBJECT

private:
    struct NODISCARD Group final
    {
        bool exclusive = false;
        QList<UiCommand *> members;
    };

    QMap<QString, UiCommand *> m_commands;
    QMap<QString, Group> m_groups;
    bool m_bulkDisabled = false;

public:
    explicit CommandRegistry(QObject *parent);
    ~CommandRegistry() final;

    DELETE_CTORS_AND_ASSIGN_OPS(CommandRegistry);

public:
    // Creates and registers a new command. `checkable` seeds
    // UiCommand::checkable; `bulkDisablable` opts the command into
    // setBulkDisabled() (this is the set that MainWindow::disableActions()
    // used to touch directly: new/open/merge/reload/save-as/exports/exit).
    NODISCARD UiCommand *addCommand(const QString &id,
                                    bool checkable = false,
                                    bool bulkDisablable = false);

    // Q_INVOKABLE so the QML shell can look commands up by id directly from
    // QML (e.g. `commands.command("view.zoom-in")`, with `commands` exposed
    // as a context property bound to this registry); see
    // QmlShellWindow.cpp/MainShell.qml/CommandAction.qml.
    NODISCARD Q_INVOKABLE UiCommand *command(const QString &id) const
    {
        return m_commands.value(id, nullptr);
    }

    // Registers `cmd` as a member of `groupName`, creating the group on
    // first use. All members of a given group must agree on `exclusive`.
    // Exclusive groups: checking one member unchecks every other member
    // (mirrors QActionGroup::setExclusive(true) semantics, but enforced on
    // the commands rather than on QActions).
    void addToGroup(UiCommand *cmd, const QString &groupName, bool exclusive);

    // Sets UiCommand::enabled on every member of `groupName` (mirrors
    // QActionGroup::setEnabled()'s group-wide toggle, e.g. the
    // selectedRoomActGroup/selectedConnectionActGroup/infomarkGroup pattern
    // of enabling a whole cluster of actions once a selection exists).
    void setGroupEnabled(const QString &groupName, bool enabled);

    // Mirrors MainWindow::disableActions(): every bulkDisablable command's
    // effectiveEnabled becomes (enabled && !disabled). This is what backs
    // MainWindow::ActionDisabler (see mainwindow-async.h).
    void setBulkDisabled(bool disabled);

    NODISCARD bool isBulkDisabled() const { return m_bulkDisabled; }

public:
    // Binds a QAction to a UiCommand so both are views of the same state:
    //  - command.trigger() calls action->trigger(), which runs whatever
    //    handlers are connected to the action's triggered() signal -- i.e.
    //    invoking a command reuses the action's existing behavior verbatim.
    //  - command's effectiveEnabled/checked changes are pushed onto the
    //    action (action->setEnabled()/setChecked()).
    //  - the action's own changes -- explicit action->setEnabled()/
    //    setChecked() calls anywhere in MainWindow, and Qt-internal effects
    //    like QActionGroup's automatic exclusivity enforcement -- are
    //    mirrored back into the command via QAction::changed (which fires
    //    on every property change, so it catches call sites that were not
    //    individually migrated to go through the command).
    // Both directions carry a shared "syncing" guard so a change pushed one
    // way can't bounce back and re-trigger the same change the other way;
    // see CommandRegistry.cpp for details.
    static void bindQAction(QAction *action, UiCommand *command);

private:
    void enforceExclusive(const QString &groupName, const UiCommand *source);
};
