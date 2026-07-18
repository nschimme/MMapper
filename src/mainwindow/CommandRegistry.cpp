// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "CommandRegistry.h"

#include "../global/macros.h"

#include <memory>

#include <QAction>
#include <QDebug>
#include <QKeySequence>
#include <QPointer>

CommandRegistry::CommandRegistry(QObject *const parent)
    : QObject(parent)
{}

CommandRegistry::~CommandRegistry() = default;

UiCommand *CommandRegistry::addCommand(const QString &id,
                                       const bool checkable,
                                       const bool bulkDisablable)
{
    if (m_commands.contains(id)) {
        qWarning() << "CommandRegistry: duplicate command id" << id;
        return m_commands.value(id);
    }

    auto *const cmd = new UiCommand(id, this);
    cmd->setCheckable(checkable);
    cmd->m_bulkDisablable = bulkDisablable;
    if (bulkDisablable && m_bulkDisabled) {
        cmd->setBulkDisabled(true);
    }
    m_commands.insert(id, cmd);
    return cmd;
}

void CommandRegistry::addToGroup(UiCommand *const cmd,
                                 const QString &groupName,
                                 const bool exclusive)
{
    if (cmd == nullptr) {
        return;
    }

    Group &group = m_groups[groupName];
    if (group.members.isEmpty()) {
        group.exclusive = exclusive;
    } else if (group.exclusive != exclusive) {
        qWarning() << "CommandRegistry: group" << groupName
                   << "was created with exclusive=" << group.exclusive << "but" << cmd->getId()
                   << "requested" << exclusive;
    }
    group.members.append(cmd);

    if (group.exclusive) {
        QObject::connect(cmd,
                         &UiCommand::sig_checkedChanged,
                         this,
                         [this, groupName, cmd](bool checked) {
                             if (checked) {
                                 enforceExclusive(groupName, cmd);
                             }
                         });
    }
}

void CommandRegistry::setGroupEnabled(const QString &groupName, const bool enabled)
{
    const auto it = m_groups.constFind(groupName);
    if (it == m_groups.constEnd()) {
        return;
    }
    for (UiCommand *const member : it.value().members) {
        member->setEnabled(enabled);
    }
}

void CommandRegistry::enforceExclusive(const QString &groupName, const UiCommand *const source)
{
    const auto it = m_groups.constFind(groupName);
    if (it == m_groups.constEnd()) {
        return;
    }
    for (UiCommand *const member : it.value().members) {
        if (member != source && member->isChecked()) {
            member->setChecked(false);
        }
    }
}

void CommandRegistry::setBulkDisabled(const bool disabled)
{
    if (m_bulkDisabled == disabled) {
        return;
    }
    m_bulkDisabled = disabled;
    for (UiCommand *const cmd : std::as_const(m_commands)) {
        if (cmd->isBulkDisablable()) {
            cmd->setBulkDisabled(disabled);
        }
    }
}

void CommandRegistry::bindQAction(QAction *const action, UiCommand *const command)
{
    if (action == nullptr || command == nullptr) {
        return;
    }

    // One-time initial sync: by the time an action is bound it has already
    // been fully configured (text/shortcut/checkable/enabled/checked), so
    // mirror that snapshot into the command's metadata/state up front. Text
    // and icons otherwise stay owned by the QAction side (see
    // CommandRegistry.h); the command only keeps a copy for the QML shell.
    command->setText(action->text());
    command->setShortcut(action->shortcut().toString(QKeySequence::PortableText));
    command->setCheckable(action->isCheckable());
    command->setEnabled(action->isEnabled());
    command->setChecked(action->isChecked());
    action->setEnabled(command->isEffectiveEnabled());

    // Shared loop-guard: while either side is applying a change that
    // originated from the other side, the receiving handler must not echo
    // it back. A plain bool (not a QAtomic) is fine -- both the command and
    // the action live on the GUI thread and all of this happens
    // synchronously within Qt's direct-connection signal emission.
    auto guard = std::make_shared<bool>(false);

    // command -> action: invoking the command re-runs the action's existing
    // triggered() handlers verbatim.
    QObject::connect(command, &UiCommand::sig_triggered, action, [action]() { action->trigger(); });

    // command -> action: effectiveEnabled/checked follow the command.
    QPointer<QAction> weakAction(action);
    QObject::connect(command,
                     &UiCommand::sig_effectiveEnabledChanged,
                     action,
                     [weakAction, guard](bool enabled) {
                         if (*guard || weakAction.isNull()) {
                             return;
                         }
                         *guard = true;
                         weakAction->setEnabled(enabled);
                         *guard = false;
                     });
    QObject::connect(command,
                     &UiCommand::sig_checkedChanged,
                     action,
                     [weakAction, guard](bool checked) {
                         if (*guard || weakAction.isNull() || !weakAction->isCheckable()) {
                             return;
                         }
                         *guard = true;
                         weakAction->setChecked(checked);
                         *guard = false;
                     });

    // action -> command: QAction::changed fires on *any* property change,
    // so this is a safety net that also catches call sites that were not
    // individually migrated to go through the command (e.g. QActionGroup's
    // own enable/exclusivity bookkeeping). It only ever mirrors the
    // *logical* enabled flag; bulk-disable is exclusively driven by
    // CommandRegistry::setBulkDisabled(), never observed here, so it can't
    // be corrupted by this path.
    QPointer<UiCommand> weakCommand(command);
    QObject::connect(action, &QAction::changed, command, [weakAction, weakCommand, guard]() {
        if (*guard || weakAction.isNull() || weakCommand.isNull()) {
            return;
        }
        *guard = true;
        weakCommand->setEnabled(weakAction->isEnabled());
        if (weakAction->isCheckable()) {
            weakCommand->setChecked(weakAction->isChecked());
        }
        *guard = false;
    });
}
