// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Nils Schimmelmann <nschimme@gmail.com> (Jahara)

#include "mumeclockwidget.h"

#include "../configuration/configuration.h"
#include "mumeclock.h"
#include "mumemoment.h"

#include <cassert>
#include <memory>

#include <QDateTime>
#include <QLabel>
#include <QMouseEvent>
#include <QString>

MumeClockWidget::MumeClockWidget(GameObserver &observer, MumeClock *clock, QWidget *const parent)
    : QWidget(parent)
    , m_observer(observer)
    , m_clock(clock)
{
    setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);
    assert(testAttribute(Qt::WA_DeleteOnClose));

    connect(&observer, &GameObserver::timeOfDayChanged, this, &MumeClockWidget::slot_updateTime);
    connect(&observer, &GameObserver::moonPhaseChanged, this,
            &MumeClockWidget::slot_updateMoonPhase);
    connect(&observer, &GameObserver::moonVisibilityChanged, this,
            &MumeClockWidget::slot_updateMoonVisibility);
    connect(&observer, &GameObserver::seasonChanged, this, &MumeClockWidget::slot_updateSeason);
    connect(&observer, &GameObserver::countdownChanged, this,
            &MumeClockWidget::slot_updateCountdown);
    connect(&observer, &GameObserver::tick, this, &MumeClockWidget::slot_updateStatusTips);

    slot_updateTime(observer.getTimeOfDay());
    slot_updateMoonPhase(observer.getMoonPhase());
    slot_updateMoonVisibility(observer.getMoonVisibility());
    slot_updateSeason(observer.getSeason());
    slot_updateCountdown(observer.getCountdown());
    slot_updateStatusTips(clock->getMumeMoment());
}

MumeClockWidget::~MumeClockWidget() = default;

void MumeClockWidget::mousePressEvent(QMouseEvent * /*event*/)
{
    // Force precision to minute and reset last sync to current timestamp
    m_clock->setPrecision(MumeClockPrecisionEnum::MINUTE);
    m_clock->setLastSyncEpoch(QDateTime::currentDateTimeUtc().toSecsSinceEpoch());
}

void MumeClockWidget::slot_updateTime(MumeTimeEnum time)
{
    if (time != m_lastTime) {
        m_lastTime = time;
        // The current time is 12:15 am.
        QString styleSheet = "";
        QString statusTip = "";
        if (m_clock->getPrecision() <= MumeClockPrecisionEnum::UNSET) {
            styleSheet = "padding-left:1px;padding-right:1px;color:white;background:grey";
        } else if (time == MumeTimeEnum::DAWN) {
            styleSheet = "padding-left:1px;padding-right:1px;color:white;background:red";
            statusTip = "Ticks left until day";
        } else if (time >= MumeTimeEnum::DUSK) {
            styleSheet = "padding-left:1px;padding-right:1px;color:white;background:blue";
            statusTip = "Ticks left until day";
        } else {
            styleSheet = "padding-left:1px;padding-right:1px;color:black;background:yellow";
            statusTip = "Ticks left until night";
        }
        if (m_clock->getPrecision() != MumeClockPrecisionEnum::MINUTE) {
            statusTip = "The clock has not synced with MUME! Click to override at your own risk.";
        }

        timeLabel->setStyleSheet(styleSheet);
        timeLabel->setStatusTip(statusTip);
    }
}

void MumeClockWidget::slot_updateMoonPhase(MumeMoonPhaseEnum phase)
{
    if (phase != m_lastPhase) {
        m_lastPhase = phase;
        switch (phase) {
        case MumeMoonPhaseEnum::WAXING_CRESCENT:
            moonPhaseLabel->setText(QString::fromUtf8("\xF0\x9F\x8C\x92"));
            break;
        case MumeMoonPhaseEnum::FIRST_QUARTER:
            moonPhaseLabel->setText(QString::fromUtf8("\xF0\x9F\x8C\x93"));
            break;
        case MumeMoonPhaseEnum::WAXING_GIBBOUS:
            moonPhaseLabel->setText(QString::fromUtf8("\xF0\x9F\x8C\x94"));
            break;
        case MumeMoonPhaseEnum::FULL_MOON:
            moonPhaseLabel->setText(QString::fromUtf8("\xF0\x9F\x8C\x95"));
            break;
        case MumeMoonPhaseEnum::WANING_GIBBOUS:
            moonPhaseLabel->setText(QString::fromUtf8("\xF0\x9F\x8C\x96"));
            break;
        case MumeMoonPhaseEnum::THIRD_QUARTER:
            moonPhaseLabel->setText(QString::fromUtf8("\xF0\x9F\x8C\x97"));
            break;
        case MumeMoonPhaseEnum::WANING_CRESCENT:
            moonPhaseLabel->setText(QString::fromUtf8("\xF0\x9F\x8C\x98"));
            break;
        case MumeMoonPhaseEnum::NEW_MOON:
            moonPhaseLabel->setText(QString::fromUtf8("\xF0\x9F\x8C\x91"));
            break;
        case MumeMoonPhaseEnum::UNKNOWN:
            moonPhaseLabel->setText("");
            break;
        }
    }
}

void MumeClockWidget::slot_updateMoonVisibility(MumeMoonVisibilityEnum visibility)
{
    if (visibility != m_lastVisibility) {
        m_lastVisibility = visibility;
        const QString moonStyleSheet = (visibility == MumeMoonVisibilityEnum::INVISIBLE
                                        || visibility == MumeMoonVisibilityEnum::UNKNOWN)
                                           ? "color:black;background:grey"
                                       : (visibility == MumeMoonVisibilityEnum::BRIGHT)
                                           ? "color:black;background:yellow"
                                           : "color:black;background:white";
        moonPhaseLabel->setStyleSheet(moonStyleSheet);
    }
}

void MumeClockWidget::slot_updateSeason(MumeSeasonEnum season)
{
    if (season != m_lastSeason) {
        m_lastSeason = season;
        QString styleSheet = "color:black";
        QString text = "Unknown";
        switch (season) {
        case MumeSeasonEnum::WINTER:
            styleSheet = "color:black;background:white";
            text = "Winter";
            break;
        case MumeSeasonEnum::SPRING:
            styleSheet = "color:white;background:teal";
            text = "Spring";
            break;
        case MumeSeasonEnum::SUMMER:
            styleSheet = "color:white;background:green";
            text = "Summer";
            break;
        case MumeSeasonEnum::AUTUMN:
            styleSheet = "color:black;background:orange";
            text = "Autumn";
            break;
        case MumeSeasonEnum::UNKNOWN:
        default:
            break;
        }
        seasonLabel->setStyleSheet(styleSheet);
        seasonLabel->setText(text);
    }
}

void MumeClockWidget::slot_updateCountdown(const QString &text)
{
    timeLabel->setText(text);
}

void MumeClockWidget::slot_updateStatusTips(const MumeMoment &moment)
{
    moonPhaseLabel->setStatusTip(moment.toMumeMoonTime());
    seasonLabel->setStatusTip(m_clock->toMumeTime(moment));
}
