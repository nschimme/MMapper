// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "MapCanvasUnderlayItem.h"

#include <QHoverEvent>
#include <QMouseEvent>
#include <QNativeGestureEvent>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QQuickWindow>
#include <QTouchEvent>
#include <QWheelEvent>

qreal MapCanvasUnderlayItem::hostDevicePixelRatio() const
{
    return window() != nullptr ? window()->effectiveDevicePixelRatio() : 1.0;
}

MapCanvasUnderlayItem::MapCanvasUnderlayItem(QQuickItem *const parent)
    : QQuickItem(parent)
{
    // No setFlag(ItemHasContents, true): this item paints nothing through
    // the normal scene graph (it has no updatePaintNode() override, and the
    // default is already ItemHasContents == false) -- all of its actual
    // pixels come from paintUnderlay()'s direct GL calls during
    // beforeRenderPassRecording(), not from a scene graph node. That is the
    // whole point (see the file comment): MapView.qml/MainShell.qml must
    // therefore keep nothing OPAQUE stacked on top of this item's rect in
    // the ordinary Quick scene, or it will simply paint over what this item
    // just drew directly into the framebuffer.
    setAcceptedMouseButtons(Qt::AllButtons);
    setAcceptTouchEvents(true);
    setAcceptHoverEvents(true);
}

MapCanvasUnderlayItem::~MapCanvasUnderlayItem()
{
    if (m_core != nullptr) {
        m_core->setHost(nullptr);
    }
}

void MapCanvasUnderlayItem::setCore(MapCanvasCore *const core)
{
    if (m_core == core) {
        return;
    }

    if (m_core != nullptr) {
        m_core->setHost(nullptr);
    }

    m_core = core;

    if (m_core != nullptr) {
        m_core->setHost(this);
    }

    emit coreChanged();
    requestCanvasUpdate();
}

void MapCanvasUnderlayItem::requestCanvasUpdate()
{
    if (QQuickWindow *const win = window()) {
        win->update();
    }
}

void MapCanvasUnderlayItem::mousePressEvent(QMouseEvent *const event)
{
    if (m_core != nullptr) {
        m_core->handleMousePress(event);
    }
}

void MapCanvasUnderlayItem::mouseReleaseEvent(QMouseEvent *const event)
{
    if (m_core != nullptr) {
        m_core->handleMouseRelease(event);
    }
}

void MapCanvasUnderlayItem::mouseMoveEvent(QMouseEvent *const event)
{
    if (m_core != nullptr) {
        m_core->handleMouseMove(event);
    }
}

void MapCanvasUnderlayItem::hoverMoveEvent(QHoverEvent *const event)
{
    if (m_core != nullptr) {
        const QPointF pos = event->position();
        m_core->handlePointerMove(glm::vec2{static_cast<float>(pos.x()),
                                            static_cast<float>(pos.y())},
                                  event->modifiers(),
                                  Qt::NoButton);
    }
}

void MapCanvasUnderlayItem::wheelEvent(QWheelEvent *const event)
{
    if (m_core != nullptr) {
        m_core->handleWheel(event);
    }
}

void MapCanvasUnderlayItem::touchEvent(QTouchEvent *const event)
{
    if (m_core != nullptr) {
        m_core->handleTouch(event);
    }
}

bool MapCanvasUnderlayItem::event(QEvent *const event)
{
    // Mirrors MapCanvasQuickItem::event() -- see its comment.
    if (m_core != nullptr && m_core->handleGenericEvent(event)) {
        return true;
    }
    return QQuickItem::event(event);
}

void MapCanvasUnderlayItem::itemChange(const ItemChange change, const ItemChangeData &value)
{
    QQuickItem::itemChange(change, value);
    if (change == ItemSceneChange) {
        connectToWindow(value.window);
    }
}

void MapCanvasUnderlayItem::connectToWindow(QQuickWindow *const newWindow)
{
    if (m_connectedWindow == newWindow) {
        return;
    }

    if (m_connectedWindow != nullptr) {
        disconnect(m_connectedWindow,
                   &QQuickWindow::sceneGraphInvalidated,
                   this,
                   &MapCanvasUnderlayItem::handleSceneGraphInvalidated);
        disconnect(m_connectedWindow,
                   &QQuickWindow::beforeRenderPassRecording,
                   this,
                   &MapCanvasUnderlayItem::paintUnderlay);
    }

    m_connectedWindow = newWindow;

    if (m_connectedWindow != nullptr) {
        // DirectConnection: sceneGraphInvalidated() fires on the render
        // thread with the GL context current (exactly what
        // hostCleanupGL() requires) -- see MapCanvasQuickItem's identical
        // comment, and this class's own header comment on why it's the
        // ONLY cleanup path here.
        connect(m_connectedWindow,
                &QQuickWindow::sceneGraphInvalidated,
                this,
                &MapCanvasUnderlayItem::handleSceneGraphInvalidated,
                Qt::DirectConnection);
        // DirectConnection: beforeRenderPassRecording() fires on the render
        // thread (== the GUI thread under the basic loop this shell forces
        // -- see the file comment) with the window's GL context current and
        // its render pass about to record Quick's own draw commands; a
        // queued connection would defer this item's GL calls to some later
        // point outside that window, which is not what "underlay" means
        // here.
        connect(m_connectedWindow,
                &QQuickWindow::beforeRenderPassRecording,
                this,
                &MapCanvasUnderlayItem::paintUnderlay,
                Qt::DirectConnection);
    }
}

void MapCanvasUnderlayItem::handleSceneGraphInvalidated()
{
    if (m_core != nullptr && !m_core->isCleanedUp()) {
        m_core->hostCleanupGL();
    }
}

void MapCanvasUnderlayItem::paintUnderlay()
{
    if (m_core == nullptr || m_core->isCleanedUp()) {
        return;
    }

    QQuickWindow *const win = window();
    if (win == nullptr || !isVisible() || width() <= 0 || height() <= 0) {
        return;
    }

    if (!m_glInitialized) {
        m_glInitialized = m_core->hostInitializeGL();
        if (!m_glInitialized) {
            // Failure was already reported via
            // MapCanvasCore::sig_glInitFailed(); nothing more to do here.
            return;
        }
    }

    // This item's on-screen rect, in the window's PHYSICAL-pixel coordinate
    // system: mapToScene() maps this item's local origin through every
    // ancestor transform (the enclosing SplitView layout, etc.) into the
    // window's logical-pixel "scene" coordinate system (origin top-left,
    // Y-down -- ordinary Qt Quick UI coordinates), which is then scaled by
    // the effective device pixel ratio to get physical pixels.
    const qreal dpr = win->effectiveDevicePixelRatio();
    const QPointF topLeftScene = mapToScene(QPointF(0, 0));
    const int physX = qRound(topLeftScene.x() * dpr);
    const int physY = qRound(topLeftScene.y() * dpr);
    const int physW = qRound(width() * dpr);
    const int physH = qRound(height() * dpr);
    const int windowPhysicalHeight = qRound(win->height() * dpr);

    if (physW <= 0 || physH <= 0) {
        return;
    }

    const QSize physicalSize(physW, physH);
    if (physicalSize != m_lastPhysicalSize || !utils::equals(dpr, m_lastDpr)) {
        m_lastPhysicalSize = physicalSize;
        m_lastDpr = dpr;
        m_core->hostResize(physW, physH, dpr);
    }

    // OpenGL's viewport/scissor origin is bottom-left, but the rect just
    // computed above is top-left-origin/Y-down (Qt Quick's convention);
    // flip it the same way MapCanvasQuickItem historically needed
    // setMirrorVertically(true) for -- except here there is no separate
    // "item FBO that Quick later texture-samples" to compensate for (that
    // was the QQuickFramebufferObject-specific wrinkle setMirrorVertically()
    // existed for), so this is a plain coordinate-system conversion, not a
    // rendering-orientation fix: blitFboToDefault() (Legacy.cpp) composites
    // MapCanvasCore's own internal FBO into whatever framebuffer is
    // CURRENTLY BOUND via a full-screen triangle -- here, the window's own
    // backbuffer -- exactly the same target the old QOpenGLWindow-based
    // MapCanvas host (mapcanvas.h) drew into every frame with correct
    // orientation and no mirroring at all. The map's own content should
    // therefore come out right-side up here too, without any mirror flag;
    // this is the one piece of this design that can't be verified in this
    // GL-less container and needs the user's own visual check on macOS.
    const int viewportY = windowPhysicalHeight - (physY + physH);

    win->beginExternalCommands();

    if (QOpenGLContext *const ctx = QOpenGLContext::currentContext()) {
        QOpenGLFunctions *const f = ctx->functions();
        f->glViewport(physX, viewportY, physW, physH);
        f->glEnable(GL_SCISSOR_TEST);
        f->glScissor(physX, viewportY, physW, physH);

        m_core->hostPaintGL();

        f->glDisable(GL_SCISSOR_TEST);
    }

    win->endExternalCommands();
}
