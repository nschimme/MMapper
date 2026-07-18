#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "../global/RuleOf5.h"
#include "../global/macros.h"

#include <QObject>
#include <QString>
#include <QUrl>
#include <QtCore>

class CommandRegistry;

// A UiCommand is the single source of truth for one user-triggerable action
// (what is today a QAction in MainWindow) shared between the QWidgets shell
// and the future QML shell. It intentionally depends on QtCore only (no
// QtWidgets, no QtGui) so it can be used unmodified from QML.
//
// - "enabled" is the *logical* enable state, i.e. whatever the app's
//   business logic thinks the command should allow right now (e.g. "a room
//   is selected"). "effectiveEnabled" folds in the registry-wide bulk
//   disable (see CommandRegistry::setBulkDisabled) for commands that opt
//   into it via bulkDisablable. Views (QAction, QML MenuItem/Button/...)
//   should bind to effectiveEnabled, not enabled.
// - "checked"/"checkable" model toggle and exclusive-choice (radio) UI.
//   Exclusive-group enforcement (unchecking siblings) is done by
//   CommandRegistry, not by UiCommand itself.
// - trigger() is Q_INVOKABLE so QML can call cmd.trigger() directly; it just
//   emits sig_triggered(), which whoever created/bound the command connects
//   to the actual handler (see CommandRegistry::bindQAction).
class NODISCARD_QOBJECT UiCommand final : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString id READ getId CONSTANT)
    Q_PROPERTY(QString text READ getText WRITE setText NOTIFY sig_textChanged)
    Q_PROPERTY(QString shortcut READ getShortcut WRITE setShortcut NOTIFY sig_shortcutChanged)
    Q_PROPERTY(QUrl iconSource READ getIconSource WRITE setIconSource NOTIFY sig_iconSourceChanged)
    Q_PROPERTY(bool enabled READ isEnabled WRITE setEnabled NOTIFY sig_enabledChanged)
    Q_PROPERTY(bool effectiveEnabled READ isEffectiveEnabled NOTIFY sig_effectiveEnabledChanged)
    Q_PROPERTY(bool checkable READ isCheckable WRITE setCheckable NOTIFY sig_checkableChanged)
    Q_PROPERTY(bool checked READ isChecked WRITE setChecked NOTIFY sig_checkedChanged)

private:
    friend class CommandRegistry;

    QString m_id;
    QString m_text;
    QString m_shortcut;
    QUrl m_iconSource;
    bool m_enabled = true;
    bool m_bulkDisabled = false;
    bool m_checkable = false;
    bool m_checked = false;
    // Set by CommandRegistry::addCommand() for the small set of commands
    // that MainWindow::disableActions() used to touch; only those commands
    // are affected by setBulkDisabled().
    bool m_bulkDisablable = false;

public:
    explicit UiCommand(QString id, QObject *parent);
    ~UiCommand() final;

    DELETE_CTORS_AND_ASSIGN_OPS(UiCommand);

public:
    NODISCARD QString getId() const { return m_id; }

    NODISCARD QString getText() const { return m_text; }
    void setText(QString text);

    NODISCARD QString getShortcut() const { return m_shortcut; }
    void setShortcut(QString shortcut);

    NODISCARD QUrl getIconSource() const { return m_iconSource; }
    void setIconSource(QUrl url);

    NODISCARD bool isEnabled() const { return m_enabled; }
    void setEnabled(bool enabled);

    NODISCARD bool isEffectiveEnabled() const { return m_enabled && !m_bulkDisabled; }

    NODISCARD bool isCheckable() const { return m_checkable; }
    void setCheckable(bool checkable);

    NODISCARD bool isChecked() const { return m_checked; }
    void setChecked(bool checked);

    NODISCARD bool isBulkDisablable() const { return m_bulkDisablable; }

public:
    Q_INVOKABLE void trigger() { emit sig_triggered(); }

private:
    // Only CommandRegistry may flip the bulk-disable bit (setBulkDisabled()
    // applies it to every bulkDisablable command it owns).
    void setBulkDisabled(bool bulkDisabled);

signals:
    void sig_triggered();
    void sig_textChanged();
    void sig_shortcutChanged();
    void sig_iconSourceChanged();
    void sig_enabledChanged(bool enabled);
    void sig_effectiveEnabledChanged(bool effectiveEnabled);
    void sig_checkableChanged(bool checkable);
    void sig_checkedChanged(bool checked);
};
