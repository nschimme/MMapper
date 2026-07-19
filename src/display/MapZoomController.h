#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "../global/macros.h"
#include "MapCanvasData.h"

#include <QObject>

class MapCanvasCore;

// QML-facing counterpart to MapZoomSlider (see ../mainwindow/MapZoomSlider.h):
// the same logarithmic zoom<->slider-position conversion (calcPos()/SCALE),
// exposed as a plain QObject `position` property instead of a QSlider
// subclass, so Shell B's toolbar (a QQC2.Slider in MainShell.qml, not a
// QWidget) can bind against it the same way MapZoomSlider drives
// MainWindow's QToolBar widget. Deliberately a separate class rather than a
// shared base: MapZoomSlider is wired against MapWindow (a QWidget with its
// own sig_zoomChanged relay), while this binds directly against
// MapCanvasCore (Shell B has no MapWindow); sharing the math without
// sharing the QSlider/QWidget plumbing is cheaper as a small duplicate class
// than as a common template/base -- see the task report for this tradeoff.
class NODISCARD_QOBJECT MapZoomController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int position READ getPosition WRITE setPosition NOTIFY sig_positionChanged)
    Q_PROPERTY(int minimum READ getMinimum CONSTANT)
    Q_PROPERTY(int maximum READ getMaximum CONSTANT)

private:
    static constexpr float SCALE = 100.f;
    static constexpr float INV_SCALE = 1.f / SCALE;

    NODISCARD static int calcPos(float zoom) noexcept;
    static inline const int s_min = calcPos(ScaleFactor::MIN_VALUE);
    static inline const int s_max = calcPos(ScaleFactor::MAX_VALUE);

    MapCanvasCore &m_canvas;
    int m_position = 0;

public:
    explicit MapZoomController(MapCanvasCore &canvas, QObject *parent = nullptr);

public:
    NODISCARD int getPosition() const { return m_position; }
    void setPosition(int position);

    NODISCARD static int getMinimum() { return s_min; }
    NODISCARD static int getMaximum() { return s_max; }

signals:
    void sig_positionChanged(int position);

private:
    void setFromActual();
    NODISCARD static int clamp(int val) { return std::clamp(val, s_min, s_max); }
};
