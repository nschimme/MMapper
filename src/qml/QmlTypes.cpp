// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "QmlTypes.h"

#include "../display/MapCanvasQuickItem.h"
#include "../display/MapCanvasUnderlayItem.h"

#include <QQmlEngine>

void registerMmQmlTypes()
{
    // MapCanvasQuickItem (a QQuickFramebufferObject) is the
    // MMAPPER_CANVAS_FBO=1 fallback path; MapCanvasUnderlayItem (a bare
    // QQuickItem hooking QQuickWindow::beforeRenderPassRecording()) is the
    // default, performance end-state path -- see MapView.qml and
    // QmlShellWindow.cpp for the "useCanvasUnderlay" selection, and
    // MapCanvasUnderlayItem.h's file comment for why it exists.
    qmlRegisterType<MapCanvasQuickItem>("MMapper", 1, 0, "MapCanvasItem");
    qmlRegisterType<MapCanvasUnderlayItem>("MMapper", 1, 0, "MapCanvasUnderlay");
}
