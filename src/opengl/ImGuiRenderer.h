#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#include "GLText.h"

#include <imgui.h>
#include <vector>

#include <QObject>
#include <QPointF>

struct MapCanvasViewport;
class QMouseEvent;
class QWheelEvent;
class QKeyEvent;
class QOpenGLWindow;

class ImGuiRenderer : public QObject
{
    Q_OBJECT
public:
    explicit ImGuiRenderer(QOpenGLWindow *window);
    ~ImGuiRenderer() override;

    void initialize();
    void updateDPI();
    void setDPIDirty() { m_dpiDirty = true; }
    void newFrame();
    void render();

    void draw2dText(const std::vector<GLText> &text);
    void draw3dText(const MapCanvasViewport &viewport, const std::vector<GLText> &text);

    bool handleMouseEvent(QMouseEvent *event);
    bool handleWheelEvent(QWheelEvent *event);
    bool handleKeyEvent(QKeyEvent *event);

private:
    void updateModifiers();

    QOpenGLWindow *m_window;
    bool m_initialized = false;
    bool m_dpiDirty = false;

    ImFont *m_fontRegular = nullptr;
    ImFont *m_fontItalic = nullptr;
};
