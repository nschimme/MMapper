// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "ClockAdapter.h"

#include "../configuration/configuration.h"
#include "ClockStrings.h"

#include <cmath>

#include <QCursor>
#include <QDateTime>
#include <QPoint>
#include <QToolTip>
#include <QWidget>

ClockAdapter::ClockAdapter(GameObserver &observer, MumeClock &clock, QObject *const parent)
    : QObject(parent)
    , m_clock(clock)
{
    observer.sig2_timeOfDayChanged.connect(m_lifetime,
                                           [this](const MumeTimeEnum time) { updateTime(time); });
    observer.sig2_moonPhaseChanged.connect(m_lifetime, [this](MumeMoonPhaseEnum phase) {
        updateMoonPhase(phase);
    });
    observer.sig2_moonVisibilityChanged.connect(m_lifetime,
                                                [this](const MumeMoonVisibilityEnum visibility) {
                                                    updateMoonVisibility(visibility);
                                                });
    observer.sig2_seasonChanged.connect(m_lifetime, [this](const MumeSeasonEnum season) {
        updateSeason(season);
    });
    observer.sig2_weatherChanged.connect(m_lifetime, [this](const PromptWeatherEnum weather) {
        updateWeather(weather);
    });
    observer.sig2_fogChanged.connect(m_lifetime,
                                     [this](const PromptFogEnum fog) { updateFog(fog); });
    observer.sig2_tick.connect(m_lifetime,
                               [this](const MumeMoment &moment) { updateCountdown(moment); });

    auto moment = m_clock.getMumeMoment();
    updateTime(moment.toTimeOfDay());
    updateMoonPhase(moment.moonPhase());
    updateMoonVisibility(moment.moonVisibility());
    updateSeason(moment.toSeason());
    updateCountdown(moment);
    updateWeather(PromptWeatherEnum::NICE);
    updateFog(PromptFogEnum::NO_FOG);
}

void ClockAdapter::clicked()
{
    // Force precision to minute and reset last sync to current timestamp.
    m_clock.setPrecision(MumeClockPrecisionEnum::MINUTE);
    m_clock.setLastSyncEpoch(QDateTime::currentDateTimeUtc().toSecsSinceEpoch());
    auto moment = m_clock.getMumeMoment();
    updateTime(moment.toTimeOfDay());
    updateCountdown(moment);
}

void ClockAdapter::showToolTip(const QString &text, const qreal x, const qreal y)
{
    const QPoint global = m_toolTipWidget ? m_toolTipWidget->mapToGlobal(
                                                QPoint(static_cast<int>(std::lround(x)),
                                                       static_cast<int>(std::lround(y))))
                                          : QCursor::pos();
    QToolTip::showText(global, text, m_toolTipWidget);
}

void ClockAdapter::hideToolTip()
{
    QToolTip::hideText();
}

void ClockAdapter::updateTime(const MumeTimeEnum time)
{
    // The current time is 12:15 am.
    if (m_clock.getPrecision() <= MumeClockPrecisionEnum::UNSET) {
        m_countdownFgColor = Qt::white;
        m_countdownBgColor = Qt::gray;
    } else if (time == MumeTimeEnum::DAWN) {
        m_countdownFgColor = Qt::white;
        m_countdownBgColor = Qt::red;
    } else if (time >= MumeTimeEnum::DUSK) {
        m_countdownFgColor = Qt::white;
        m_countdownBgColor = Qt::blue;
    } else {
        m_countdownFgColor = Qt::black;
        m_countdownBgColor = Qt::yellow;
    }
    emit sig_changed();
}

void ClockAdapter::updateMoonPhase(const MumeMoonPhaseEnum phase)
{
    m_moonText = clockstrings::moonPhaseEmoji(phase);
    emit sig_changed();
}

void ClockAdapter::updateMoonVisibility(const MumeMoonVisibilityEnum visibility)
{
    m_moonBgColor = (visibility == MumeMoonVisibilityEnum::INVISIBLE
                     || visibility == MumeMoonVisibilityEnum::UNKNOWN)
                        ? QColor(Qt::gray)
                    : (visibility == MumeMoonVisibilityEnum::BRIGHT) ? QColor(Qt::yellow)
                                                                     : QColor(Qt::white);
    emit sig_changed();
}

void ClockAdapter::updateSeason(const MumeSeasonEnum season)
{
    switch (season) {
    case MumeSeasonEnum::WINTER:
        m_seasonFgColor = Qt::black;
        m_seasonBgColor = Qt::white;
        break;
    case MumeSeasonEnum::SPRING:
        m_seasonFgColor = Qt::white;
        m_seasonBgColor = QColor(Qt::darkCyan);
        break;
    case MumeSeasonEnum::SUMMER:
        m_seasonFgColor = Qt::white;
        m_seasonBgColor = Qt::green;
        break;
    case MumeSeasonEnum::AUTUMN:
        m_seasonFgColor = Qt::black;
        m_seasonBgColor = QColor(255, 165, 0); // orange
        break;
    case MumeSeasonEnum::UNKNOWN:
    default:
        m_seasonFgColor = Qt::black;
        m_seasonBgColor = Qt::transparent;
        break;
    }
    m_seasonText = clockstrings::seasonText(season);
    emit sig_changed();
}

void ClockAdapter::updateWeather(const PromptWeatherEnum weather)
{
    m_weatherText = clockstrings::weatherEmoji(weather);
    m_weatherTooltip = clockstrings::weatherTooltip(weather);
    m_weatherVisible = !m_weatherText.isEmpty();
    emit sig_changed();
}

void ClockAdapter::updateFog(const PromptFogEnum fog)
{
    m_fogText = clockstrings::fogEmoji(fog);
    m_fogTooltip = clockstrings::fogTooltip(fog);
    m_fogVisible = !m_fogText.isEmpty();
    emit sig_changed();
}

void ClockAdapter::updateCountdown(const MumeMoment &moment)
{
    // FIXME: Use ChangeMonitor
    m_shown = getConfig().mumeClock.display;
    if (!m_shown) {
        emit sig_changed();
        return;
    }

    const MumeClockPrecisionEnum precision = m_clock.getPrecision();
    if (precision <= MumeClockPrecisionEnum::HOUR) {
        m_countdownText = QString::fromUtf8("\xE2\x9A\xA0").append(m_clock.toCountdown(moment));
    } else {
        m_countdownText = m_clock.toCountdown(moment);
    }

    updateStatusTips(moment);
    emit sig_changed();
}

void ClockAdapter::updateStatusTips(const MumeMoment &moment)
{
    m_moonTooltip = moment.toMumeMoonTime();
    m_seasonTooltip = m_clock.toMumeTime(moment);

    QString tooltip;
    const MumeClockPrecisionEnum precision = m_clock.getPrecision();
    const auto time = moment.toTimeOfDay();
    if (time == MumeTimeEnum::DAWN) {
        tooltip = "Ticks left until day";
    } else if (time >= MumeTimeEnum::DUSK) {
        tooltip = "Ticks left until day";
    } else {
        tooltip = "Ticks left until night";
    }
    if (precision != MumeClockPrecisionEnum::MINUTE) {
        tooltip = "The clock has not synced with MUME! Click to override at your own risk.";
    }
    m_countdownTooltip = tooltip;
}
