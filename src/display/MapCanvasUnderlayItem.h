#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

// MapCanvasUnderlayItem is the performance end-state host for MapCanvasCore:
// it draws the map DIRECTLY into the window's own default framebuffer,
// UNDER the rest of the Qt Quick scene, via
// QQuickWindow::beforeRenderPassRecording(). This eliminates the extra
// offscreen-FBO-and-composite pass MapCanvasQuickItem (the
// QQuickFramebufferObject host, see MapCanvasQuickItem.h) requires every
// frame: MapCanvasQuickItem renders the map into its OWN item FBO, which Qt
// Quick then resolves/textures into the window as an ordinary scene graph
// node -- a whole extra full-screen render pass plus a texture round-trip
// on top of what MapCanvasCore itself already does (its own internal
// multisampled FBO -> resolve -> full-screen-triangle blit, see
// Legacy::Functions::blitFboToDefault()). This item instead has NO scene
// graph content of its own (a bare QQuickItem, ItemHasContents left at its
// default false): it hooks the window's render pass directly and lets
// MapCanvasCore's own blitFboToDefault() composite straight into the
// framebuffer Quick itself is about to draw into, with the map's on-screen
// rect enforced via glViewport()/glScissor() computed from this item's
// current window-space geometry. Quick's subsequent draws of the rest of
// the scene (docks, toolbars, splash overlay, ...) then simply layer on top
// in the same render pass, painter's algorithm style -- see MapView.qml's
// and MainShell.qml's file comments for the "transparent hole" bookkeeping
// this requires from the rest of the scene.
//
// Selected by MapView.qml (see its own doc comment) whenever the
// "useCanvasUnderlay" root context property is true or unset, which is
// QmlShellWindow's default; the MMAPPER_CANVAS_FBO=1 environment variable
// forces MapCanvasQuickItem (the QQuickFramebufferObject path) instead --
// see QmlShellWindow.cpp. Both classes stay compiled in permanently: this
// class is the intended end state, but MapCanvasQuickItem remains available
// as a fallback (e.g. for a Qt Quick backend/platform combination where
// beforeRenderPassRecording()-based external GL commands misbehave).
//
// Unlike MapCanvas (mapcanvas.h) and like MapCanvasQuickItem, this item does
// NOT own its MapCanvasCore -- see MapCanvasQuickItem.h's file comment,
// which applies here verbatim (including "every code path here must
// tolerate a null core").
//
// Threading: same single-threaded ("basic" QML render loop) assumption as
// MapCanvasQuickItem -- see its file comment for why. Under the basic loop,
// beforeRenderPassRecording() fires on the GUI thread (render thread ==
// GUI thread), so this class -- like MapCanvasQuickItem -- contains no
// cross-thread synchronization. A threaded render loop would need this
// revisited: paintUnderlay() would then run on a separate render thread
// from the GUI thread that owns this item's geometry/`core` property, and
// every read of `this` (geometry, m_core, m_lastPhysicalSize/m_lastDpr)
// inside it would need the same kind of GUI/render-thread handoff
// QQuickFramebufferObject::Renderer::synchronize() gives MapCanvasQuickItem
// today.
#include "../global/macros.h"
#include "MapCanvasCore.h"

#include <QPointer>
#include <QQuickItem>
#include <QSize>

class QHoverEvent;
class QMouseEvent;
class QNativeGestureEvent;
class QQuickWindow;
class QTouchEvent;
class QWheelEvent;

// Not `final`: qmlRegisterType<MapCanvasUnderlayItem>() (see
// ../qml/QmlTypes.cpp) instantiates
// QQmlPrivate::QQmlElement<MapCanvasUnderlayItem>, which itself derives from
// MapCanvasUnderlayItem -- see MapCanvasQuickItem.h's identical comment for
// why a `final` class can't be a QML-creatable type.
class NODISCARD_QOBJECT MapCanvasUnderlayItem : public QQuickItem, private ICanvasHost
{
    Q_OBJECT

    // See MapCanvasQuickItem.h's identical Q_PROPERTY doc comment: the item
    // does not own the core, and a null core is always valid.
    Q_PROPERTY(MapCanvasCore *core READ getCore WRITE setCore NOTIFY coreChanged)

public:
    explicit MapCanvasUnderlayItem(QQuickItem *parent = nullptr);
    ~MapCanvasUnderlayItem() override;

public:
    NODISCARD MapCanvasCore *getCore() const { return m_core; }
    void setCore(MapCanvasCore *core);

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
    void requestCanvasUpdate() override;
    // Defined out-of-line in the .cpp for the same reason as
    // MapCanvasQuickItem::hostDevicePixelRatio() -- see its comment.
    NODISCARD qreal hostDevicePixelRatio() const override;
    NODISCARD QSize hostSize() const override { return QSize(qRound(width()), qRound(height())); }
    void setHostCursor(Qt::CursorShape shape) override { QQuickItem::setCursor(shape); }

private:
    // Backstop cleanup path: connected (DirectConnection) to the current
    // window's sceneGraphInvalidated() whenever this item has a window (see
    // itemChange()) -- identical reasoning to MapCanvasQuickItem's own
    // connectToWindow()/handleSceneGraphInvalidated(), except here it is the
    // ONLY cleanup path: unlike MapCanvasQuickItem (which also has its
    // Renderer's destructor, guaranteed a current GL context by Qt Quick's
    // QQuickFramebufferObject machinery), this item has no Renderer of its
    // own, so sceneGraphInvalidated() is the sole place hostCleanupGL() ever
    // runs. That is sufficient in practice: QmlShellWindow's destructor
    // (see the "engine-first" comment there, added in 7f54017) deletes the
    // QQmlApplicationEngine -- which tears down the whole QML scene graph,
    // firing sceneGraphInvalidated() with a current GL context -- strictly
    // before MapCanvasCore is destroyed.
    void handleSceneGraphInvalidated();
    void connectToWindow(QQuickWindow *window);

    // Connected to the window's beforeRenderPassRecording() (DirectConnection;
    // see the file comment for why that's safe under the basic render
    // loop). Does the actual GL work: lazily calls hostInitializeGL(),
    // computes this item's current window-space rect (in physical pixels)
    // and sets glViewport()/glScissor() to it, calls hostResize() when that
    // rect's size (or the DPR) changed since the last call, then
    // hostPaintGL(). Brackets the GL calls in
    // QQuickWindow::beginExternalCommands()/endExternalCommands(), which is
    // how the Qt Quick documentation says external GL/RHI-native code
    // should interleave with a render pass Quick itself is driving.
    void paintUnderlay();

private:
    QPointer<MapCanvasCore> m_core;
    QPointer<QQuickWindow> m_connectedWindow;
    bool m_glInitialized = false;

    // The physical-pixel size (and DPR) last passed to
    // MapCanvasCore::hostResize() by paintUnderlay(), so it's only called
    // again when the item's on-screen size (or the window's DPR) actually
    // changes -- mirroring MapCanvasQuickItemRenderer::synchronize()'s
    // once-per-synchronize call, adapted to "once per change" since this
    // class has no separate synchronize() phase to hook: paintUnderlay()
    // runs every frame regardless of whether anything changed.
    QSize m_lastPhysicalSize;
    qreal m_lastDpr = 0.0;

signals:
    void coreChanged();
};
