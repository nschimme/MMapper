#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "../global/macros.h"

#include <QObject>

// Q_PROPERTY façade over Configuration::PathMachineSettings (see
// ../configuration/configuration.h), mirroring pathmachinepage.cpp's
// apply-on-change semantics: every setter writes setConfig().pathMachine.*
// immediately. PathMachineSettings has no ChangeMonitor, so reload() (called
// by PreferencesController::reloadAll()) simply re-emits sig_changed
// unconditionally to resync any bound QML after an external mutation (e.g.
// Cancel's setConfig().read()).
class NODISCARD_QOBJECT PathMachinePageAdapter final : public QObject
{
    Q_OBJECT

    Q_PROPERTY(double acceptBestRelative READ getAcceptBestRelative WRITE setAcceptBestRelative
                   NOTIFY sig_changed)
    Q_PROPERTY(double acceptBestAbsolute READ getAcceptBestAbsolute WRITE setAcceptBestAbsolute
                   NOTIFY sig_changed)
    Q_PROPERTY(
        double newRoomPenalty READ getNewRoomPenalty WRITE setNewRoomPenalty NOTIFY sig_changed)
    Q_PROPERTY(double correctPositionBonus READ getCorrectPositionBonus WRITE
                   setCorrectPositionBonus NOTIFY sig_changed)
    Q_PROPERTY(double multipleConnectionsPenalty READ getMultipleConnectionsPenalty WRITE
                   setMultipleConnectionsPenalty NOTIFY sig_changed)
    Q_PROPERTY(int maxPaths READ getMaxPaths WRITE setMaxPaths NOTIFY sig_changed)
    Q_PROPERTY(int matchingTolerance READ getMatchingTolerance WRITE setMatchingTolerance NOTIFY
                   sig_changed)

public:
    explicit PathMachinePageAdapter(QObject *parent);

public:
    NODISCARD double getAcceptBestRelative() const;
    void setAcceptBestRelative(double value);

    NODISCARD double getAcceptBestAbsolute() const;
    void setAcceptBestAbsolute(double value);

    NODISCARD double getNewRoomPenalty() const;
    void setNewRoomPenalty(double value);

    NODISCARD double getCorrectPositionBonus() const;
    void setCorrectPositionBonus(double value);

    NODISCARD double getMultipleConnectionsPenalty() const;
    void setMultipleConnectionsPenalty(double value);

    NODISCARD int getMaxPaths() const;
    void setMaxPaths(int value);

    NODISCARD int getMatchingTolerance() const;
    void setMatchingTolerance(int value);

public slots:
    // Re-emits sig_changed so QML bindings resync against the live
    // Configuration; must be called whenever code outside this class may
    // have written to getConfig().pathMachine (e.g. Cancel).
    void reload();

signals:
    void sig_changed();
};
