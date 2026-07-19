#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "../global/ConfigEnums.h"
#include "../global/macros.h"

#include <QObject>
#include <QString>

class HotkeyManager;

// Abstract seam Proxy/ConnectionListener use to reach back into whichever
// shell owns them (MainWindow or QmlShellWindow) without depending on the
// concrete widget-shell type MainWindow. Both shells implement this via
// (non-exclusive) multiple inheritance alongside their own QObject base --
// see mainwindow.h's `class MainWindow final : public QMainWindow, public
// ProxyHostApi` and QmlShellWindow.h's equivalent. Proxy previously
// dynamic_cast<MainWindow*>'d ConnectionListener::parent() directly (see
// proxy.cpp's old getMainWindow() free function); that made it impossible
// to construct a ConnectionListener/Proxy graph under any shell other than
// MainWindow. ConnectionListener's parent must now implement this interface
// instead (see proxy.cpp's getProxyHost()).
class NODISCARD ProxyHostApi
{
public:
    virtual ~ProxyHostApi() = default;

public:
    // MainWindow::slot_log() / QmlShellWindow::slot_log(): funnels
    // Proxy/parser log messages ("mod", "message") into whichever log
    // surface the host shell has (statusBar() + LogModel dock for
    // MainWindow, just LogModel for QmlShellWindow).
    virtual void slot_log(const QString &mod, const QString &msg) = 0;

    // MainWindow::slot_setMode() / QmlShellWindow's equivalent: the mud can
    // request a mapper-mode switch via the MPI protocol (see
    // ProxyMudConnectionApi::virt_onSetMode() in proxy.cpp); both shells
    // apply it the same way a user clicking the mapper-mode toolbar buttons
    // would.
    virtual void slot_setMode(MapModeEnum mode) = 0;

    // MainWindow::getHotkeyManager() / QmlShellWindow's equivalent: the
    // parser needs this to expand user-defined hotkey aliases.
    NODISCARD virtual HotkeyManager &getHotkeyManager() const = 0;

    // RemoteEdit needs a QObject parent to outlive the Proxy that spawned it
    // (see Proxy::allocRemoteEdit()'s "Caution: RemoteEdit outlives the
    // proxy" comment); this exposes the host shell's own QObject identity
    // for that purpose. Implementations should just `return *this;`.
    NODISCARD virtual QObject &asQObject() = 0;
};
