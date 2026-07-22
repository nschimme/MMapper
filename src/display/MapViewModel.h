#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

// MapViewModel holds the map view's scroll math (world<->scroll-unit
// conversion, driven by the known map bounds) and its continuous-scroll
// timer, extracted out of MapWindow (mapwindow.h/.cpp) so the same logic can
// eventually back both MapWindow (QScrollBar-based) and a future
// MapView.qml (QQC2 ScrollBar-based). It owns no widgets and no
// QQuickItem-related state, so it belongs in the unconditional display
// sources (built in both the WITH_QML and non-QML configurations).
//
// This commit only extracts the math/timer and refactors MapWindow to use
// it (see mapwindow.cpp); MapWindow keeps owning its QScrollBar widgets and
// the "read bar value, compute delta, write bar value" glue, since that
// part is inherently QWidget-specific. A future QML shell commit will wire
// MapView.qml's ScrollBars directly against this same model.
#include "../global/macros.h"
#include "../map/coordinate.h"

#include <glm/glm.hpp>

#include <QObject>
#include <QPointer>

class QTimer;

class NODISCARD_QOBJECT MapViewModel final : public QObject
{
    Q_OBJECT

    Q_PROPERTY(int horizontalScrollMax READ getHorizontalScrollMax NOTIFY sig_rangeChanged)
    Q_PROPERTY(int verticalScrollMax READ getVerticalScrollMax NOTIFY sig_rangeChanged)
    Q_PROPERTY(float zoom READ getZoom WRITE setZoom NOTIFY sig_zoomChanged)
    // Current scroll-bar values (scroll units, matching horizontalScrollMax/
    // verticalScrollMax's range), mirroring what used to live directly on
    // MapWindow's QScrollBar widgets (see mapwindow.cpp's
    // m_horizontalScrollBar/m_verticalScrollBar). MapView.qml's QQC2.ScrollBar
    // instances bind their `position` to these (scaled by *ScrollMax) and
    // write back through the WRITE setters on drag -- see MapView.qml's
    // onMoved handlers.
    Q_PROPERTY(int horizontalScrollValue READ getHorizontalScrollValue WRITE
                   setHorizontalScrollValue NOTIFY sig_valueChanged)
    Q_PROPERTY(int verticalScrollValue READ getVerticalScrollValue WRITE setVerticalScrollValue
                   NOTIFY sig_valueChanged)

public:
    // Must match MapCanvasCore::SCROLL_SCALE (display/MapCanvasCore.h);
    // duplicated here (rather than depending on MapCanvasCore.h, which drags
    // in the entire OpenGL/rendering graph) so this model stays a small,
    // independently-testable QObject with no rendering dependencies.
    static constexpr const int SCROLL_SCALE = 64;

private:
    // Ported verbatim from MapWindow::KnownMapSize (see mapwindow.h/.cpp);
    // this is the same world<->scroll-unit conversion, just no longer
    // private to MapWindow.
    struct NODISCARD KnownMapSize final
    {
        glm::ivec3 min{0};
        glm::ivec3 max{0};

        NODISCARD glm::ivec2 size() const { return glm::ivec2{max - min}; }

        NODISCARD glm::vec2 scrollToWorld(glm::ivec2 scrollPos) const;
        NODISCARD glm::ivec2 worldToScroll(glm::vec2 worldPos) const;
    } m_knownMapSize;

    QPointer<QTimer> m_scrollTimer;
    int m_horizontalScrollStep = 0;
    int m_verticalScrollStep = 0;
    int m_horizontalScrollMax = 0;
    int m_verticalScrollMax = 0;
    int m_horizontalScrollValue = 0;
    int m_verticalScrollValue = 0;
    float m_zoom = 1.f;

public:
    explicit MapViewModel(QObject *parent = nullptr);
    ~MapViewModel() final;

public:
    NODISCARD glm::vec2 scrollToWorld(glm::ivec2 scrollPos) const
    {
        return m_knownMapSize.scrollToWorld(scrollPos);
    }
    NODISCARD glm::ivec2 worldToScroll(glm::vec2 worldPos) const
    {
        return m_knownMapSize.worldToScroll(worldPos);
    }

    NODISCARD int getHorizontalScrollMax() const { return m_horizontalScrollMax; }
    NODISCARD int getVerticalScrollMax() const { return m_verticalScrollMax; }

    NODISCARD int getHorizontalScrollValue() const { return m_horizontalScrollValue; }
    NODISCARD int getVerticalScrollValue() const { return m_verticalScrollValue; }

    NODISCARD float getZoom() const { return m_zoom; }
    void setZoom(float zoom);

public slots:
    // Ported from MapWindow::slot_setScrollBars()/updateScrollBars(): stores
    // the known map bounds and recomputes the scrollbar ranges (min is
    // always 0, matching the original QScrollBar::setRange(0, dims.*) calls).
    void slot_setScrollBars(Coordinate min, Coordinate max);

    // Ported from MapWindow::slot_continuousScroll()/slot_scrollTimerTimeout():
    // starts/stops a 100ms-interval timer while a non-zero step is active,
    // and emits sig_continuousScrollStep() on every tick. See mapwindow.cpp's
    // original comment: "This looks more like 'delayed jump' than
    // 'continuous scroll.'"
    void slot_continuousScroll(int hStep, int vStep);

    // Ported from MapWindow::slot_centerOnWorldPos(): converts a world
    // position (MapCanvasCore::sig_onCenter) to scroll-bar units, updates
    // horizontalScrollValue/verticalScrollValue (clamped to the current
    // range, mirroring QScrollBar::setValue()'s own clamping), and emits
    // sig_scrollToWorld() with the ORIGINAL (unclamped) world position --
    // exactly what MapWindow::slot_centerOnWorldPos() re-emits as
    // sig_setScroll(worldPos) for MapCanvas::slot_setScroll() to consume
    // (mapwindow.cpp:257-269).
    void slot_centerOnWorldPos(glm::vec2 worldPos);

    // Ported from MapWindow::slot_mapMove() (mapwindow.cpp:216-231): applies
    // a canvas-drag delta to the current scroll position (Y negated, since
    // the delta arrives in world space) and emits sig_scrollToWorld() with
    // the resulting world position.
    void slot_mapMove(int dx, int dy);

    // WRITE half of horizontalScrollValue/verticalScrollValue -- used by
    // MapView.qml's ScrollBar.onMoved (user dragging the QML scrollbar
    // handle directly), mirroring MapWindow's per-axis
    // `connect(m_horizontalScrollBar, &QScrollBar::valueChanged, ...)` /
    // `connect(m_verticalScrollBar, &QScrollBar::valueChanged, ...)` lambdas
    // (mapwindow.cpp:153-167), which likewise update only one axis of the
    // canvas's scroll position at a time via
    // slot_setHorizontalScroll()/slot_setVerticalScroll() rather than the
    // combined slot_setScroll().
    void setHorizontalScrollValue(int value);
    void setVerticalScrollValue(int value);

signals:
    void sig_rangeChanged();
    void sig_zoomChanged(float zoom);
    // Emitted every 100ms while a continuous scroll is active (see
    // slot_continuousScroll()); applied internally by this class (see the
    // .cpp) the same way MapWindow::slot_applyScrollStep() applied it to its
    // QScrollBar widgets (mapwindow.cpp:238-250).
    void sig_continuousScrollStep(int hStep, int vStep);

    // NOTIFY signal for horizontalScrollValue/verticalScrollValue.
    void sig_valueChanged();

    // Emitted whenever the scroll position moves as a two-axis world
    // position (slot_centerOnWorldPos()/slot_mapMove()/the continuous-scroll
    // timer) -- connect to MapCanvasCore::slot_setScroll(), mirroring
    // MapWindow::sig_setScroll() -> MapCanvas::slot_setScroll()
    // (mapwindow.cpp:169).
    void sig_scrollToWorld(glm::vec2 worldPos);

    // Emitted by setHorizontalScrollValue()/setVerticalScrollValue() with
    // just the single axis's new world coordinate -- connect to
    // MapCanvasCore::slot_setHorizontalScroll()/slot_setVerticalScroll(),
    // mirroring the per-axis QScrollBar::valueChanged lambdas above.
    void sig_scrollWorldXChanged(float worldX);
    void sig_scrollWorldYChanged(float worldY);

private slots:
    void slot_scrollTimerTimeout();

private:
    // Shared by slot_mapMove() and the continuous-scroll timer's internal
    // sig_continuousScrollStep consumer: adds a delta already expressed in
    // scroll-bar units to the current position, clamps to range, and emits
    // sig_valueChanged()/sig_scrollToWorld() -- ported from the shared
    // "SignalBlocker + setValue() + centerOnScrollPos()" pattern in
    // MapWindow::slot_mapMove()/slot_applyScrollStep() (mapwindow.cpp:216-
    // 250).
    void applyScrollDelta(int dh, int dv);
};
