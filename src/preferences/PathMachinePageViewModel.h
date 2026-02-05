#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#include "../global/macros.h"
#include <QObject>

class NODISCARD_QOBJECT PathMachinePageViewModel final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(double acceptBestRelative READ acceptBestRelative WRITE setAcceptBestRelative NOTIFY settingsChanged)
    Q_PROPERTY(double acceptBestAbsolute READ acceptBestAbsolute WRITE setAcceptBestAbsolute NOTIFY settingsChanged)
    Q_PROPERTY(double newRoomPenalty READ newRoomPenalty WRITE setNewRoomPenalty NOTIFY settingsChanged)
    Q_PROPERTY(double multipleConnectionsPenalty READ multipleConnectionsPenalty WRITE setMultipleConnectionsPenalty NOTIFY settingsChanged)
    Q_PROPERTY(double correctPositionBonus READ correctPositionBonus WRITE setCorrectPositionBonus NOTIFY settingsChanged)
    Q_PROPERTY(int maxPaths READ maxPaths WRITE setMaxPaths NOTIFY settingsChanged)
    Q_PROPERTY(int matchingTolerance READ matchingTolerance WRITE setMatchingTolerance NOTIFY settingsChanged)

public:
    explicit PathMachinePageViewModel(QObject *parent = nullptr);

    NODISCARD double acceptBestRelative() const; void setAcceptBestRelative(double v);
    NODISCARD double acceptBestAbsolute() const; void setAcceptBestAbsolute(double v);
    NODISCARD double newRoomPenalty() const; void setNewRoomPenalty(double v);
    NODISCARD double multipleConnectionsPenalty() const; void setMultipleConnectionsPenalty(double v);
    NODISCARD double correctPositionBonus() const; void setCorrectPositionBonus(double v);
    NODISCARD int maxPaths() const; void setMaxPaths(int v);
    NODISCARD int matchingTolerance() const; void setMatchingTolerance(int v);

    void loadConfig();

signals:
    void settingsChanged();
};
