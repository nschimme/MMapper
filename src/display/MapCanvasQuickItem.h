#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

// MapCanvasQuickItem is the QQuickFramebufferObject-based host for
// MapCanvasCore, mirroring the role MapCanvas (mapcanvas.h) plays for the
// QOpenGLWindow path. It is currently INERT: nothing in the running app
// constructs one yet (it is only registered as a QML type -- see
// ../qml/QmlTypes.h -- for a future QML shell to use). The QOpenGLWindow
// path (MapCanvas/MapWindow) remains the active UI.
//
// Unlike MapCanvas, this item does NOT own its MapCanvasCore: the future
// shell creates/owns the core alongside MapData, and binds it to this item
// via the `core` property. Every code path here must tolerate a null core
// (no core bound yet, or the shell tore it down) by simply no-op'ing.
//
// Threading: this item assumes the basic (single-threaded) QML render loop,
// i.e. GUI thread == render thread, which is what the future shell will
// select at startup (see main.cpp). It deliberately contains no threading
// machinery (no mutexes, no cross-thread queuing) -- Renderer::synchronize()
// simply reads the GUI-thread item's state directly. If a threaded render
// loop is ever introduced, this class will need to be revisited.
#include "../global/macros.h"
#include "MapCanvasCore.h"

#include <QPointer>
#include <QQuickFramebufferObject>

class QHoverEvent;
class QMouseEvent;
class QNativeGestureEvent;
class QTouchEvent;
class QWheelEvent;

// Not `final`: qmlRegisterType<MapCanvasQuickItem>() (see ../qml/QmlTypes.cpp)
// instantiates QQmlPrivate::QQmlElement<MapCanvasQuickItem>, which itself
// derives from MapCanvasQuickItem -- a `final` class can't be a QML-creatable
// type.
class NODISCARD_QOBJECT MapCanvasQuickItem : public QQuickFramebufferObject, private ICanvasHost
{
    Q_OBJECT

    // The item does not own the core (see the file comment above); the
    // future shell is expected to construct MapCanvasCore alongside MapData
    // and bind it here. A null core is always valid and simply means
    // "nothing to render/forward events to yet."
    Q_PROPERTY(MapCanvasCore *core READ getCore WRITE setCore NOTIFY coreChanged)

public:
    explicit MapCanvasQuickItem(QQuickItem *parent = nullptr);
    ~MapCanvasQuickItem() override;

public:
    NODISCARD MapCanvasCore *getCore() const { return m_core; }
    void setCore(MapCanvasCore *core);

public:
    NODISCARD Renderer *createRenderer() const override;

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void hoverMoveEvent(QHoverEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void touchEvent(QTouchEvent *event) override;
    bool event(QEvent *event) override;

    void itemChange(ItemChange change, const ItemChangeData &value) override;

private:
    // ICanvasHost
    void requestCanvasUpdate() override { update(); }
    // Defined out-of-line in the .cpp: QQuickItem::window() returns
    // QQuickWindow*, which is only forward-declared by <QQuickItem> (pulled
    // in transitively via <QQuickFramebufferObject>); calling
    // effectiveDevicePixelRatio() on it needs the complete type, which only
    // <QQuickWindow> (included in the .cpp) provides.
    NODISCARD qreal hostDevicePixelRatio() const override;
    NODISCARD QSize hostSize() const override { return QSize(qRound(width()), qRound(height())); }
    void setHostCursor(Qt::CursorShape shape) override { QQuickItem::setCursor(shape); }

private:
    // Backstop cleanup path: connected (DirectConnection) to the current
    // window's sceneGraphInvalidated() whenever this item has a window (see
    // itemChange()). Runs on the render thread with a current GL context
    // (that's the point in the scene graph's lifecycle where this signal
    // fires), so it's a safe place to call MapCanvasCore::hostCleanupGL()
    // even if the Renderer that would normally do this in its destructor
    // never got a chance to run (e.g. an abrupt scene graph teardown).
    void handleSceneGraphInvalidated();
    void connectToWindow(QQuickWindow *window);

private:
    QPointer<MapCanvasCore> m_core;
    QPointer<QQuickWindow> m_connectedWindow;

signals:
    void coreChanged();
};
