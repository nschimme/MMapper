#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "../display/CanvasMouseModeEnum.h"
#include "../global/ConfigEnums.h"
#include "../global/macros.h"
#include "../mapdata/roomselection.h"

#include <QObject>
#include <QString>

class CommandRegistry;
class ConnectionSelection;
class InfomarkSelection;
class MapCanvas;
class MapData;
class Mmapper2PathMachine;

// AppCore holds the shell-agnostic slice of MainWindow's application logic
// that has been extracted so far (see mainwindow/mainwindow.cpp for what
// still lives there). It is QtWidgets-free (QtCore/QtGui only) so it can
// eventually be driven by a QML shell instead of MainWindow. MainWindow
// constructs one AppCore and owns it; AppCore only holds non-owning
// references/pointers to objects MainWindow continues to own.
//
// Everything shell-visible comes back out through signals rather than
// AppCore reaching into widgets directly:
//   sig_statusMessage        -- MainWindow forwards this to statusBar()
//   sig_mapperModeChanged    -- MainWindow updates the mode menu icon/check
//
// Scope note: this is a partial extraction (see the AppCore extraction
// commit message / task report for the full list of what remains in
// MainWindow, e.g. file I/O + the async task engine + close/quit
// coordination, which are still too entangled with QProgressDialog and
// QFileDialog/QMessageBox to move safely in this slice).
class NODISCARD_QOBJECT AppCore final : public QObject
{
    Q_OBJECT

private:
    MapData &m_mapData;
    Mmapper2PathMachine &m_pathMachine;
    CommandRegistry &m_commandRegistry;
    MapCanvas *m_canvas = nullptr;

public:
    explicit AppCore(MapData &mapData,
                     Mmapper2PathMachine &pathMachine,
                     CommandRegistry &commandRegistry,
                     QObject *parent);
    ~AppCore() final;

    DELETE_CTORS_AND_ASSIGN_OPS(AppCore);

public:
    // The canvas doesn't exist yet when AppCore is constructed in
    // MainWindow's ctor (MapWindow/MapCanvas setup order); MainWindow calls
    // this once it does, before wiring any canvas-facing slots to AppCore.
    void setCanvas(MapCanvas *canvas) { m_canvas = canvas; }

public:
    // --- status-message funnel (slice 4) ---
    // Replaces MainWindow::showStatusInternal()/showStatusShort()/
    // showStatusLong()/showStatusForever(): callers ask AppCore to show a
    // status message, AppCore emits sig_statusMessage, and MainWindow's
    // sole remaining widget-touching responsibility is forwarding that to
    // statusBar()->showMessage().
    void showStatusShort(const QString &txt) { showStatusInternal(txt, 2000); }
    void showStatusLong(const QString &txt) { showStatusInternal(txt, 5000); }
    void showStatusForever(const QString &txt) { showStatusInternal(txt, 0); }

public:
    // --- mapper mode switching (slice 2) ---
    // Persists the mode to config and emits sig_mapperModeChanged;
    // MainWindow's slot_onPlayMode()/slot_onMapMode()/slot_onOfflineMode()
    // still own logging and updating the mode menu's icon/checked action.
    void setMapperMode(MapModeEnum mode);

public:
    // --- mouse mode + layer switching, canvas-facing calls (slice 2) ---
    void setCanvasMouseMode(CanvasMouseModeEnum mode);
    void layerUp();
    void layerDown();
    void layerReset();

public:
    // --- selection -> command enabling (slice 3) ---
    // MainWindow continues to own the actual selection storage (it's read
    // by widget-facing code like the context menu), but delegates the
    // "what does this selection change enable/disable, and what status
    // message does it produce" logic here.

    // Mirrors the original MainWindow::slot_newRoomSelection() body.
    void onNewRoomSelection(const SigRoomSelection &rs);

    // Mirrors the original MainWindow::slot_newConnectionSelection() body.
    void onNewConnectionSelection(bool hasSelection);

    // Mirrors the original MainWindow::slot_newInfomarkSelection() body,
    // minus opening the infomark editor (a dialog, which stays shell-side):
    // returns true when MainWindow should open it, exactly when the
    // original code would have called slot_onEditInfomarkSelection().
    NODISCARD bool onNewInfomarkSelection(bool hasSelection, size_t size);

    // Mirrors the sig_selectionChanged lambda from
    // MainWindow::wireConnections(): re-enables the move/merge up/down
    // room commands based on whether an adjacent-layer room exists under
    // the current selection.
    void updateRoomOffsetCommands(const SharedRoomSelection &roomSelection);

signals:
    void sig_statusMessage(const QString &text, int durationMs);
    void sig_mapperModeChanged(MapModeEnum mode);

private:
    void showStatusInternal(const QString &txt, int durationMs);
};
