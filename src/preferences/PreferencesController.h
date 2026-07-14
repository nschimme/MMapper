#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "../global/macros.h"

#include <QObject>
#include <QPointer>
#include <QWidget>

class PathMachinePageAdapter;
class MumeProtocolPageAdapter;
class AutoLogPageAdapter;
class GroupPageAdapter;
class AudioPageAdapter;

// QML-facing controller for the preferences dialog, mirroring ConfigDialog's
// (see configdialog.h/.cpp) apply-on-change + OK/Cancel semantics without
// owning any widgets itself.
//
// This round (Phase 6, Commits 10-11 of the QML migration) only ports the
// five SIMPLE pages (Path Machine, Mume Protocol, Auto Logger, Group
// Manager, Audio); the other four pages (General, Graphics, Parser,
// Integrated Client) still live only in the widget ConfigDialog and have no
// QML adapter yet. Deliberately only 5 adapter properties are declared here
// (not 9 with 4 left null) so PreferencesDialog.qml can never null-deref an
// unimplemented page.
//
// Each adapter is created as a child of this controller and is given
// dialogParent so its native pickers (QFileDialog/QColorDialog) parent
// correctly to the hosting window, mirroring how ConfigDialog's pages pass
// `this` to their own pickers.
//
// ok()/cancel() mirror ConfigDialog::slot_ok()/slot_cancel(): OK writes the
// live Configuration to disk and accepts; Cancel re-reads Configuration from
// disk (discarding any apply-on-change writes made while the dialog was
// open) and reloads every adapter so QML resyncs.
class NODISCARD_QOBJECT PreferencesController final : public QObject
{
    Q_OBJECT

    Q_PROPERTY(PathMachinePageAdapter *pathMachine READ getPathMachine CONSTANT)
    Q_PROPERTY(MumeProtocolPageAdapter *mumeProtocol READ getMumeProtocol CONSTANT)
    Q_PROPERTY(AutoLogPageAdapter *autoLog READ getAutoLog CONSTANT)
    Q_PROPERTY(GroupPageAdapter *group READ getGroup CONSTANT)
    Q_PROPERTY(AudioPageAdapter *audio READ getAudio CONSTANT)

private:
    QPointer<QWidget> m_dialogParent;
    PathMachinePageAdapter *m_pathMachine = nullptr;
    MumeProtocolPageAdapter *m_mumeProtocol = nullptr;
    AutoLogPageAdapter *m_autoLog = nullptr;
    GroupPageAdapter *m_group = nullptr;
    AudioPageAdapter *m_audio = nullptr;

public:
    explicit PreferencesController(QWidget *dialogParent, QObject *parent = nullptr);

public:
    NODISCARD PathMachinePageAdapter *getPathMachine() const { return m_pathMachine; }
    NODISCARD MumeProtocolPageAdapter *getMumeProtocol() const { return m_mumeProtocol; }
    NODISCARD AutoLogPageAdapter *getAutoLog() const { return m_autoLog; }
    NODISCARD GroupPageAdapter *getGroup() const { return m_group; }
    NODISCARD AudioPageAdapter *getAudio() const { return m_audio; }

    NODISCARD QWidget *dialogParent() const { return m_dialogParent; }

public slots:
    // Mirrors ConfigDialog::slot_ok(): writes the live Configuration to disk.
    Q_INVOKABLE void ok();

    // Mirrors ConfigDialog::slot_cancel(): re-reads Configuration from disk
    // and reloads every adapter.
    Q_INVOKABLE void cancel();

    // Re-syncs every adapter against the live Configuration; mirrors
    // ConfigDialog::sig_loadConfig()'s fan-out (see configdialog.cpp).
    void reloadAll();

signals:
    // Declared for parity with ConfigDialog's signal surface and for the
    // Graphics page to hook into once it is ported; unused until then.
    void sig_graphicsSettingsChanged();

    // Forwarded from GroupPageAdapter::sig_groupSettingsChanged (see
    // GroupPageAdapter.h); mirrors ConfigDialog's GroupPage ->
    // ConfigDialog relay so MainWindow can wire this the same way it wires
    // ConfigDialog::sig_groupSettingsChanged once this controller is
    // switched in.
    void sig_groupSettingsChanged();

    // Emitted after ok() writes and accepts.
    void sig_accepted();
};
