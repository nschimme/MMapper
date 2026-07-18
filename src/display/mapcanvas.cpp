// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Ulf Hermann <ulfonk_mennhar@gmx.de> (Alve)
// Author: Marek Krejza <krejza@gmail.com> (Caligor)
// Author: Nils Schimmelmann <nschimme@gmail.com> (Jahara)

#include "mapcanvas.h"

#include "../global/ConfigConsts-Computed.h"
#include "../global/utils.h"
#include "InfomarkSelection.h"
#include "connectionselection.h"

#include <QApplication>
#include <QDesktopServices>
#include <QMessageBox>
#include <QUrl>

using NonOwningPointer = MapCanvas *;
NODISCARD static NonOwningPointer &primaryMapCanvas()
{
    static NonOwningPointer primary = nullptr;
    return primary;
}

MapCanvas::MapCanvas(MapData &mapData,
                     GameObserver &observer,
                     PrespammedPath &prespammedPath,
                     Mmapper2Group &groupManager,
                     QWindow *const parent)
    : QOpenGLWindow{NoPartialUpdate, parent}
    , m_core{mapData, observer, prespammedPath, groupManager, this}
{
    // Forward the core's public signal surface as our own, so that existing
    // callers (MainWindow, MapWindow, ...) can keep connecting to
    // `MapCanvas::sig_*` exactly as before, without knowing MapCanvasCore
    // exists.
    connect(&m_core, &MapCanvasCore::sig_onCenter, this, &MapCanvas::sig_onCenter);
    connect(&m_core, &MapCanvasCore::sig_mapMove, this, &MapCanvas::sig_mapMove);
    connect(&m_core, &MapCanvasCore::sig_setScrollBars, this, &MapCanvas::sig_setScrollBars);
    connect(&m_core, &MapCanvasCore::sig_continuousScroll, this, &MapCanvas::sig_continuousScroll);
    connect(&m_core, &MapCanvasCore::sig_log, this, &MapCanvas::sig_log);
    connect(&m_core, &MapCanvasCore::sig_selectionChanged, this, &MapCanvas::sig_selectionChanged);
    connect(&m_core, &MapCanvasCore::sig_newRoomSelection, this, &MapCanvas::sig_newRoomSelection);
    connect(&m_core,
            &MapCanvasCore::sig_newConnectionSelection,
            this,
            &MapCanvas::sig_newConnectionSelection);
    connect(&m_core,
            &MapCanvasCore::sig_newInfomarkSelection,
            this,
            &MapCanvas::sig_newInfomarkSelection);
    connect(&m_core, &MapCanvasCore::sig_setCurrentRoom, this, &MapCanvas::sig_setCurrentRoom);
    connect(&m_core, &MapCanvasCore::sig_zoomChanged, this, &MapCanvas::sig_zoomChanged);
    connect(&m_core, &MapCanvasCore::sig_showTooltip, this, &MapCanvas::sig_showTooltip);
    connect(&m_core,
            &MapCanvasCore::sig_customContextMenuRequested,
            this,
            &MapCanvas::sig_customContextMenuRequested);
    connect(&m_core,
            &MapCanvasCore::sig_dismissContextMenu,
            this,
            &MapCanvas::sig_dismissContextMenu);

    // The core stays QtWidgets-free; these two connections are where the
    // widget-specific UX (message boxes, hiding the window, aborting) lives,
    // preserving the exact behavior MapCanvas had before this split.
    connect(&m_core,
            &MapCanvasCore::sig_glInitFailed,
            this,
            &MapCanvas::handleGlInitFailed,
            Qt::DirectConnection);
    connect(&m_core,
            &MapCanvasCore::sig_glFatalError,
            this,
            &MapCanvas::handleGlFatalError,
            Qt::DirectConnection);

    m_core.setHost(this);

    NonOwningPointer &pmc = primaryMapCanvas();
    if (pmc == nullptr) {
        pmc = this;
    }
}

MapCanvas::~MapCanvas()
{
    NonOwningPointer &pmc = primaryMapCanvas();
    if (pmc == this) {
        pmc = nullptr;
    }
}

MapCanvas *MapCanvas::getPrimary()
{
    return primaryMapCanvas();
}

void MapCanvas::handleGlInitFailed(const QString &reason)
{
    qWarning() << "OpenGL initialization failed:" << reason;
    hide();
    doneCurrent();
    QMessageBox::critical(QApplication::activeWindow(),
                          "Unable to initialize OpenGL",
                          "Upgrade your video card drivers");
    if constexpr (CURRENT_PLATFORM == PlatformEnum::Windows) {
        // Link to Microsoft OpenGL Compatibility Pack
        QDesktopServices::openUrl(
            QUrl(QStringLiteral("ms-windows-store://pdp/?productid=9nqpsl29bfff")));
    }
}

void MapCanvas::handleGlFatalError(const QString &message)
{
    QMessageBox box;
    box.setWindowTitle("Fatal OpenGL error");
    box.setText(message);
    box.exec();

    std::abort();
}
