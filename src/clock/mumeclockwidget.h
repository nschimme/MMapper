#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Nils Schimmelmann <nschimme@gmail.com> (Jahara)

#include "../observer/gameobserver.h"
#include "mumeclock.h"
#include "mumemoment.h"
#include "ui_mumeclockwidget.h"

#include <memory>

#include <QString>
#include <QWidget>
#include <QtCore>

#include "../global/Signal2.h"
class QMouseEvent;
class QObject;

class MumeClockWidget final : public QWidget, private Ui::MumeClockWidget
{
private:
    GameObserver &m_observer;
    MumeClock &m_clock;
    Signal2Lifetime m_lifetime;

    MumeTimeEnum m_lastTime = MumeTimeEnum::UNKNOWN;
    MumeSeasonEnum m_lastSeason = MumeSeasonEnum::UNKNOWN;
    MumeMoonPhaseEnum m_lastPhase = MumeMoonPhaseEnum::UNKNOWN;
    MumeMoonVisibilityEnum m_lastVisibility = MumeMoonVisibilityEnum::UNKNOWN;
    MumeClockPrecisionEnum m_lastPrecision = MumeClockPrecisionEnum::UNSET;

public:
    explicit MumeClockWidget(GameObserver &observer, MumeClock &clock, QWidget *parent);
    ~MumeClockWidget() final;

protected:
    void mousePressEvent(QMouseEvent *event) override;

private:
    void updateTimeStyle(MumeTimeEnum time);
    void updateTime(MumeTimeEnum time);
    void updateMoonPhase(MumeMoonPhaseEnum phase);
    void updateMoonVisibility(MumeMoonVisibilityEnum visibility);
    void updateSeason(MumeSeasonEnum season);
    void updateStatusTips(const MumeMoment &moment);
};
