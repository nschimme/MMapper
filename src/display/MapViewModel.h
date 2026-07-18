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

signals:
    void sig_rangeChanged();
    void sig_zoomChanged(float zoom);
    // Emitted every 100ms while a continuous scroll is active (see
    // slot_continuousScroll()); the consumer (MapWindow, or a future
    // MapView.qml controller) is responsible for applying the step to
    // whatever holds the current scroll position.
    void sig_continuousScrollStep(int hStep, int vStep);

private slots:
    void slot_scrollTimerTimeout();
};
