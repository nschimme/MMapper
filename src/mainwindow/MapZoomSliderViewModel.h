#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#include "../global/macros.h"
#include <QObject>

class NODISCARD_QOBJECT MapZoomSliderViewModel final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int sliderValue READ sliderValue WRITE setSliderValue NOTIFY sliderValueChanged)
    Q_PROPERTY(float zoomValue READ zoomValue WRITE setZoomValue NOTIFY zoomValueChanged)

public:
    explicit MapZoomSliderViewModel(QObject *parent = nullptr);

    NODISCARD int sliderValue() const { return m_sliderValue; }
    void setSliderValue(int v);
    NODISCARD float zoomValue() const { return m_zoomValue; }
    void setZoomValue(float z);

signals:
    void sliderValueChanged();
    void zoomValueChanged();

private:
    int m_sliderValue = 0;
    float m_zoomValue = 1.0f;
};
