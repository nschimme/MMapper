#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "../global/RuleOf5.h"
#include "../global/macros.h"

#include <memory>

#include <QObject>
#include <QString>

class CommandRegistry;
class GameObserver;
class MapCanvasCore;
class MapData;
class MapViewModel;
class Mmapper2Group;
class PrespammedPath;
class QQmlApplicationEngine;
class UiCommand;

// QmlShellWindow bootstraps Shell B, the --qml-shell preview described in
// main.cpp's setSurfaceFormat()/main() (search for MMAPPER_QML_SHELL): a
// minimal, OFFLINE-only bring-up of enough services for MapCanvasCore to
// render and be navigated, a SUBSET of CommandRegistry commands wired to
// real slots, and a QQmlApplicationEngine loading
// qrc:/qt/qml/MMapper/MainShell.qml (source at ../qml/shell/MainShell.qml;
// see src/CMakeLists.txt's QT_RESOURCE_ALIAS comment for why it's aliased
// flat despite living in a "shell/" subdirectory on disk).
//
// Deliberately NOT constructed here (unlike MainWindow): the async task
// engine, the telnet proxy/listener, MPI/remote-edit, the group manager's
// network side, RoomManager/GMCP parsing, and all file I/O (open/save/
// merge/export). This is a pragmatic first bootable slice, not a feature-
// complete shell -- see the task report for the full list of what's still
// missing relative to MainWindow.
//
// Owns everything it constructs via plain Qt parent/child (like MainWindow
// does), so its destructor is trivial.
class NODISCARD_QOBJECT QmlShellWindow final : public QObject
{
    Q_OBJECT

public:
    explicit QmlShellWindow(QObject *parent);
    ~QmlShellWindow() final;

    DELETE_CTORS_AND_ASSIGN_OPS(QmlShellWindow);

public:
    // False if MainShell.qml failed to load (e.g. the qrc resource is
    // missing, or a QML syntax error) -- main() should treat that as fatal,
    // matching how setSurfaceFormat() failures are handled.
    NODISCARD bool isValid() const { return m_valid; }

private:
    void registerCommands();
    void wireMouseModeCommand(const QString &id, int mode);

private:
    // --- minimal offline service set (see file comment) ---
    MapData *m_mapData = nullptr;
    PrespammedPath *m_prespammedPath = nullptr;
    Mmapper2Group *m_groupManager = nullptr;
    // GameObserver is not a QObject (see observer/gameobserver.h), so it
    // can't be parented like the other services above; owned via unique_ptr
    // instead, mirroring MainWindow::m_gameObserver.
    std::unique_ptr<GameObserver> m_gameObserver;

    MapCanvasCore *m_mapCanvasCore = nullptr;
    MapViewModel *m_mapViewModel = nullptr;
    CommandRegistry *m_commandRegistry = nullptr;

    QQmlApplicationEngine *m_engine = nullptr;
    bool m_valid = false;
};
