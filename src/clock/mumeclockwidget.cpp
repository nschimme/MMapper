// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Nils Schimmelmann <nschimme@gmail.com> (Jahara)

#include "mumeclockwidget.h"

#include "../configuration/configuration.h"
#include "mumeclock.h"
#include "mumemoment.h"

#include <cassert>

#include <QDateTime>
#include <QLabel>
#include <QMouseEvent>
#include <QString>

MumeClockWidget::MumeClockWidget(GameObserver &observer, MumeClock &clock, QWidget *const parent)
    : QWidget(parent)
    , m_clock(clock)
{
    setupUi(this);
    moonPhaseLabel->setText("");
    seasonLabel->setText("");
    setAttribute(Qt::WA_DeleteOnClose);
    assert(testAttribute(Qt::WA_DeleteOnClose));
    setAttribute(Qt::WA_Hover, true);
    assert(testAttribute(Qt::WA_Hover));

    observer.sig2_timeOfDayChanged.connect(m_lifetime,
                                           [this](MumeTimeEnum time) { updateTime(time); });
    observer.sig2_moonPhaseChanged.connect(m_lifetime, [this](MumeMoonPhaseEnum phase) {
        updateMoonPhase(phase);
    });
    observer.sig2_moonVisibilityChanged.connect(m_lifetime,
                                                [this](MumeMoonVisibilityEnum visibility) {
                                                    updateMoonVisibility(visibility);
                                                });
    observer.sig2_seasonChanged.connect(m_lifetime,
                                        [this](MumeSeasonEnum season) { updateSeason(season); });
    observer.sig2_weatherChanged.connect(m_lifetime, [this](PromptWeatherEnum weather) {
        updateWeather(weather);
    });
    observer.sig2_fogChanged.connect(m_lifetime, [this](PromptFogEnum fog) { updateFog(fog); });
    observer.sig2_tick.connect(m_lifetime,
                               [this](const MumeMoment &moment) { updateCountdown(moment); });

    auto moment = m_clock.getMumeMoment();
    updateTime(moment.toTimeOfDay());
    updateMoonPhase(moment.moonPhase());
    updateMoonVisibility(moment.moonVisibility());
    updateSeason(moment.toSeason());
    updateWeather(observer.getWeather());
    updateFog(observer.getFog());
    updateCountdown(moment);
}

MumeClockWidget::~MumeClockWidget() = default;

void MumeClockWidget::mousePressEvent(QMouseEvent * /*event*/)
{
    // Force precision to minute and reset last sync to current timestamp
    m_clock.setPrecision(MumeClockPrecisionEnum::MINUTE);
    m_clock.setLastSyncEpoch(QDateTime::currentDateTimeUtc().toSecsSinceEpoch());
    auto moment = m_clock.getMumeMoment();
    updateTime(moment.toTimeOfDay());
    updateCountdown(moment);
    updateStatusTips(moment);
}

bool MumeClockWidget::event(QEvent *event)
{
    if (event->type() == QEvent::HoverEnter) {
        updateStatusTips(m_clock.getMumeMoment());
    }
    return QWidget::event(event);
}

void MumeClockWidget::updateTime(MumeTimeEnum time)
{
    // The current time is 12:15 am.
    QString styleSheet = "";
    if (m_clock.getPrecision() <= MumeClockPrecisionEnum::UNSET) {
        styleSheet = "padding-left:1px;padding-right:1px;color:white;background:grey";
    } else if (time == MumeTimeEnum::DAWN) {
        styleSheet = "padding-left:1px;padding-right:1px;color:white;background:red";
    } else if (time >= MumeTimeEnum::DUSK) {
        styleSheet = "padding-left:1px;padding-right:1px;color:white;background:blue";
    } else {
        styleSheet = "padding-left:1px;padding-right:1px;color:black;background:yellow";
    }

    timeLabel->setStyleSheet(styleSheet);
}

void MumeClockWidget::updateMoonPhase(MumeMoonPhaseEnum phase)
{
    switch (phase) {
    case MumeMoonPhaseEnum::WAXING_CRESCENT:
        moonPhaseLabel->setText(QStringLiteral("\U0001F312"));
        break;
    case MumeMoonPhaseEnum::FIRST_QUARTER:
        moonPhaseLabel->setText(QStringLiteral("\U0001F313"));
        break;
    case MumeMoonPhaseEnum::WAXING_GIBBOUS:
        moonPhaseLabel->setText(QStringLiteral("\U0001F314"));
        break;
    case MumeMoonPhaseEnum::FULL_MOON:
        moonPhaseLabel->setText(QStringLiteral("\U0001F315"));
        break;
    case MumeMoonPhaseEnum::WANING_GIBBOUS:
        moonPhaseLabel->setText(QStringLiteral("\U0001F316"));
        break;
    case MumeMoonPhaseEnum::THIRD_QUARTER:
        moonPhaseLabel->setText(QStringLiteral("\U0001F317"));
        break;
    case MumeMoonPhaseEnum::WANING_CRESCENT:
        moonPhaseLabel->setText(QStringLiteral("\U0001F318"));
        break;
    case MumeMoonPhaseEnum::NEW_MOON:
        moonPhaseLabel->setText(QStringLiteral("\U0001F311"));
        break;
    case MumeMoonPhaseEnum::UNKNOWN:
        moonPhaseLabel->setText(QString());
        break;
    }
}

void MumeClockWidget::updateMoonVisibility(MumeMoonVisibilityEnum visibility)
{
    const QString moonStyleSheet = (visibility == MumeMoonVisibilityEnum::INVISIBLE
                                    || visibility == MumeMoonVisibilityEnum::UNKNOWN)
                                       ? "color:black;background:grey"
                                   : (visibility == MumeMoonVisibilityEnum::BRIGHT)
                                       ? "color:black;background:yellow"
                                       : "color:black;background:white";
    moonPhaseLabel->setStyleSheet(moonStyleSheet);
}

void MumeClockWidget::updateSeason(MumeSeasonEnum season)
{
    QString styleSheet = "color:black";
    QString text = "";
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

void MumeClockWidget::updateWeather(PromptWeatherEnum weather)
{
    switch (weather) {
    case PromptWeatherEnum::CLOUDS:
        weatherLabel->setText(QStringLiteral("\u2601"));
        weatherLabel->setStatusTip("Cloudy");
        weatherLabel->setVisible(true);
        break;
    case PromptWeatherEnum::RAIN:
        weatherLabel->setText(QStringLiteral("\U0001F327"));
        weatherLabel->setStatusTip("Rainy");
        weatherLabel->setVisible(true);
        break;
    case PromptWeatherEnum::HEAVY_RAIN:
        weatherLabel->setText(QStringLiteral("\u26C8"));
        weatherLabel->setStatusTip("Heavy Rain");
        weatherLabel->setVisible(true);
        break;
    case PromptWeatherEnum::SNOW:
        weatherLabel->setText(QStringLiteral("\u2744"));
        weatherLabel->setStatusTip("Snowy");
        weatherLabel->setVisible(true);
        break;
    case PromptWeatherEnum::NICE:
    default:
        weatherLabel->setText(QString());
        weatherLabel->setStatusTip(QString());
        weatherLabel->setVisible(false);
        break;
    }
}

void MumeClockWidget::updateFog(PromptFogEnum fog)
{
    switch (fog) {
    case PromptFogEnum::LIGHT_FOG:
        fogLabel->setText(QStringLiteral("\U0001F32B"));
        fogLabel->setStatusTip("Light Fog");
        fogLabel->setVisible(true);
        break;
    case PromptFogEnum::HEAVY_FOG:
        fogLabel->setText(QStringLiteral("\U0001F32B\U0001F32B"));
        fogLabel->setStatusTip("Heavy Fog");
        fogLabel->setVisible(true);
        break;
    case PromptFogEnum::NO_FOG:
    default:
        fogLabel->setText(QString());
        fogLabel->setStatusTip(QString());
        fogLabel->setVisible(false);
        break;
    }
}

void MumeClockWidget::updateCountdown(const MumeMoment &moment)
{
    // FIXME: Use ChangeMonitor
    setVisible(getConfig().mumeClock.display);
    if (!getConfig().mumeClock.display) {
        return;
    }

    const MumeClockPrecisionEnum precision = m_clock.getPrecision();
    if (precision <= MumeClockPrecisionEnum::HOUR) {
        timeLabel->setText(QStringLiteral("\u26A0").append(m_clock.toCountdown(moment)));
    } else {
        timeLabel->setText(m_clock.toCountdown(moment));
    }
}

void MumeClockWidget::updateStatusTips(const MumeMoment &moment)
{
    moonPhaseLabel->setStatusTip(moment.toMumeMoonTime());
    seasonLabel->setStatusTip(m_clock.toMumeTime(moment));
    // weatherLabel and fogLabel tooltips are updated in their respective update functions.

    QString statusTip = "";
    const MumeClockPrecisionEnum precision = m_clock.getPrecision();
    const auto time = moment.toTimeOfDay();
    if (time == MumeTimeEnum::DAWN) {
        statusTip = "Ticks left until day";
    } else if (time >= MumeTimeEnum::DUSK) {
        statusTip = "Ticks left until day";
    } else {
        statusTip = "Ticks left until night";
    }
    if (precision != MumeClockPrecisionEnum::MINUTE) {
        statusTip = "The clock has not synced with MUME! Click to override at your own risk.";
    }
    timeLabel->setStatusTip(statusTip);
}
