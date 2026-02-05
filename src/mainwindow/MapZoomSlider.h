#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019-2024 The MMapper Authors

#include "MapZoomSliderViewModel.h"
#include "../display/MapCanvasData.h"
#include "../global/macros.h"
#include <QSlider>

class MapWindow;

class NODISCARD_QOBJECT MapZoomSlider final : public QSlider
{
    Q_OBJECT
private:
    MapZoomSliderViewModel m_viewModel;
    MapWindow &m_map;
    static int calcPos(const float zoom) noexcept;

public:
    explicit MapZoomSlider(MapWindow &map, QWidget *parent = nullptr);
    ~MapZoomSlider() final;

    void requestChange();
    void setFromActual();
};
