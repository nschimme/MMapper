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

class QMouseEvent;
class QObject;

class NODISCARD_QOBJECT MumeClockWidget final : public QWidget, private Ui::MumeClockWidget
{
    Q_OBJECT

private:
    GameObserver &m_observer;
    MumeClock *m_clock;

    MumeTimeEnum m_lastTime = MumeTimeEnum::UNKNOWN;
    MumeSeasonEnum m_lastSeason = MumeSeasonEnum::UNKNOWN;
    MumeMoonPhaseEnum m_lastPhase = MumeMoonPhaseEnum::UNKNOWN;
    MumeMoonVisibilityEnum m_lastVisibility = MumeMoonVisibilityEnum::UNKNOWN;
    MumeClockPrecisionEnum m_lastPrecision = MumeClockPrecisionEnum::UNSET;

public:
    explicit MumeClockWidget(GameObserver &observer, MumeClock *clock, QWidget *parent);
    ~MumeClockWidget() final;

protected:
    void mousePressEvent(QMouseEvent *event) override;

private:
    void updateTimeStyle(MumeTimeEnum time);

public slots:
    void slot_updateTime(MumeTimeEnum time);
    void slot_updateMoonPhase(MumeMoonPhaseEnum phase);
    void slot_updateMoonVisibility(MumeMoonVisibilityEnum visibility);
    void slot_updateSeason(MumeSeasonEnum season);
    void slot_updateCountdown(const QString &text);
    void slot_updateStatusTips(const MumeMoment &moment);
};
