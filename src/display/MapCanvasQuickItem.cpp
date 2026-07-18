// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "MapCanvasQuickItem.h"

#include <QHoverEvent>
#include <QMouseEvent>
#include <QNativeGestureEvent>
#include <QOpenGLFramebufferObject>
#include <QQuickOpenGLUtils>
#include <QQuickWindow>
#include <QTouchEvent>
#include <QWheelEvent>

namespace { // anonymous

// The QQuickFramebufferObject::Renderer subclass, created on demand by
// createRenderer() below (which the item's owning QQuickWindow calls once it
// actually needs to render this item -- never, under the software scene
// graph backend main.cpp forces today; see MapCanvasQuickItem.h's file
// comment). Runs on whatever thread Qt Quick's render loop uses; per this
// item's single-threaded-render-loop assumption, that's the GUI thread.
class NODISCARD MapCanvasQuickItemRenderer final : public QQuickFramebufferObject::Renderer
{
public:
    MapCanvasQuickItemRenderer() = default;

    ~MapCanvasQuickItemRenderer() final
    {
        // Qt Quick guarantees the FBO's GL context is current while a
        // Renderer is being destroyed, which is exactly what
        // hostCleanupGL() requires.
        if (m_core != nullptr && !m_core->isCleanedUp()) {
            m_core->hostCleanupGL();
        }
    }

protected:
    NODISCARD QOpenGLFramebufferObject *createFramebufferObject(const QSize &size) override
    {
        // samples=0: the application's own top-level FBO/surface keeps its
        // own MSAA; this item's FBO doesn't need a second layer of it.
        QOpenGLFramebufferObjectFormat format;
        format.setSamples(0);
        return new QOpenGLFramebufferObject(size, format);
    }

    void render() override
    {
        if (m_core == nullptr) {
            // No core bound yet (or ever) -- nothing to draw. The item's FBO
            // was already cleared to transparent/black by Qt Quick.
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

        m_core->hostPaintGL();

        // MapCanvasCore's GL calls leave the QRhi/QSG-managed GL state
        // however they please; hand it back in the shape Qt Quick expects
        // before it resumes drawing the rest of the scene.
        QQuickOpenGLUtils::resetOpenGLState();
    }

    void synchronize(QQuickFramebufferObject *item) override
    {
        auto *const mapItem = static_cast<MapCanvasQuickItem *>(item);
        m_core = mapItem->getCore();
        if (m_core == nullptr) {
            return;
        }

        const qreal dpr = item->window() != nullptr ? item->window()->effectiveDevicePixelRatio()
                                                    : 1.0;
        m_core->hostResize(qRound(item->width()), qRound(item->height()), dpr);
    }

private:
    // Latched every synchronize() call (which Qt Quick runs on the GUI
    // thread with the render thread blocked), so a plain pointer -- not a
    // QPointer -- is fine: it's never read from any other thread, and it's
    // always refreshed before the next render().
    MapCanvasCore *m_core = nullptr;
    bool m_glInitialized = false;
};

} // namespace

qreal MapCanvasQuickItem::hostDevicePixelRatio() const
{
    return window() != nullptr ? window()->effectiveDevicePixelRatio() : 1.0;
}

MapCanvasQuickItem::MapCanvasQuickItem(QQuickItem *const parent)
    : QQuickFramebufferObject(parent)
{
    // QQuickFramebufferObject's item-space FBO is rendered upside down
    // relative to the rest of the scene graph unless told otherwise.
    setMirrorVertically(true);

    setAcceptedMouseButtons(Qt::AllButtons);
    setAcceptTouchEvents(true);
    setAcceptHoverEvents(true);
}

MapCanvasQuickItem::~MapCanvasQuickItem()
{
    if (m_core != nullptr) {
        m_core->setHost(nullptr);
    }
}

void MapCanvasQuickItem::setCore(MapCanvasCore *const core)
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
    update();
}

QQuickFramebufferObject::Renderer *MapCanvasQuickItem::createRenderer() const
{
    return new MapCanvasQuickItemRenderer;
}

void MapCanvasQuickItem::mousePressEvent(QMouseEvent *const event)
{
    if (m_core != nullptr) {
        m_core->handleMousePress(event);
    }
}

void MapCanvasQuickItem::mouseReleaseEvent(QMouseEvent *const event)
{
    if (m_core != nullptr) {
        m_core->handleMouseRelease(event);
    }
}

void MapCanvasQuickItem::mouseMoveEvent(QMouseEvent *const event)
{
    if (m_core != nullptr) {
        m_core->handleMouseMove(event);
    }
}

void MapCanvasQuickItem::hoverMoveEvent(QHoverEvent *const event)
{
    if (m_core != nullptr) {
        const QPointF pos = event->position();
        m_core->handlePointerMove(glm::vec2{static_cast<float>(pos.x()),
                                            static_cast<float>(pos.y())},
                                  event->modifiers(),
                                  Qt::NoButton);
    }
}

void MapCanvasQuickItem::wheelEvent(QWheelEvent *const event)
{
    if (m_core != nullptr) {
        m_core->handleWheel(event);
    }
}

void MapCanvasQuickItem::touchEvent(QTouchEvent *const event)
{
    if (m_core != nullptr) {
        m_core->handleTouch(event);
    }
}

bool MapCanvasQuickItem::event(QEvent *const event)
{
    // Mirrors MapCanvas::event() (mapcanvas.h): MapCanvasCore's
    // handleGenericEvent() itself narrows this down to recognized native
    // zoom gestures and returns false for anything else, so it's safe to
    // call unconditionally ahead of the base class.
    if (m_core != nullptr && m_core->handleGenericEvent(event)) {
        return true;
    }
    return QQuickFramebufferObject::event(event);
}

void MapCanvasQuickItem::itemChange(const ItemChange change, const ItemChangeData &value)
{
    QQuickFramebufferObject::itemChange(change, value);
    if (change == ItemSceneChange) {
        connectToWindow(value.window);
    }
}

void MapCanvasQuickItem::connectToWindow(QQuickWindow *const newWindow)
{
    if (m_connectedWindow == newWindow) {
        return;
    }

    if (m_connectedWindow != nullptr) {
        disconnect(m_connectedWindow,
                   &QQuickWindow::sceneGraphInvalidated,
                   this,
                   &MapCanvasQuickItem::handleSceneGraphInvalidated);
    }

    m_connectedWindow = newWindow;

    if (m_connectedWindow != nullptr) {
        // DirectConnection: sceneGraphInvalidated() fires on the render
        // thread with the GL context current (exactly what
        // hostCleanupGL() requires), and a queued connection would run too
        // late -- after the context is already gone.
        connect(m_connectedWindow,
                &QQuickWindow::sceneGraphInvalidated,
                this,
                &MapCanvasQuickItem::handleSceneGraphInvalidated,
                Qt::DirectConnection);
    }
}

void MapCanvasQuickItem::handleSceneGraphInvalidated()
{
    // Backstop: normally MapCanvasQuickItemRenderer's destructor already
    // handles cleanup. This covers the case where the scene graph is torn
    // down without necessarily destroying (and thus running the destructor
    // of) the current Renderer first.
    if (m_core != nullptr && !m_core->isCleanedUp()) {
        m_core->hostCleanupGL();
    }
}
