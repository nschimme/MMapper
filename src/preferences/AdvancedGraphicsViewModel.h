#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#include "../global/macros.h"
#include <QObject>

class NODISCARD_QOBJECT AdvancedGraphicsViewModel final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool showPerfStats READ showPerfStats WRITE setShowPerfStats NOTIFY settingsChanged)
    Q_PROPERTY(bool mode3d READ mode3d WRITE setMode3d NOTIFY settingsChanged)
    Q_PROPERTY(bool autoTilt READ autoTilt WRITE setAutoTilt NOTIFY settingsChanged)

public:
    explicit AdvancedGraphicsViewModel(QObject *parent = nullptr);

    NODISCARD bool showPerfStats() const; void setShowPerfStats(bool v);
    NODISCARD bool mode3d() const; void setMode3d(bool v);
    NODISCARD bool autoTilt() const; void setAutoTilt(bool v);

    void loadConfig();

signals:
    void settingsChanged();
};
