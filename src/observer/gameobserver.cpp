// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2023 The MMapper Authors
// Author: Mike Repass <mike.repass@gmail.com> (Taryn)

#include "gameobserver.h"

#include "../global/parserutils.h"

GameObserver::GameObserver(QObject *parent) : QObject(parent)
{
}

void GameObserver::observeConnected()
{
    sig2_connected.invoke();
}

void GameObserver::observeSentToMud(const QString &input)
{
    auto str = input;
    ParserUtils::removeAnsiMarksInPlace(str);
    sig2_sentToMudString.invoke(str);
}

void GameObserver::observeSentToUser(const QString &input)
{
    auto str = input;
    ParserUtils::removeAnsiMarksInPlace(str);
    sig2_sentToUserString.invoke(str);
}

void GameObserver::observeSentToUserGmcp(const GmcpMessage &m)
{
    sig2_sentToUserGmcp.invoke(m);
}

void GameObserver::observeToggledEchoMode(const bool echo)
{
    sig2_toggledEchoMode.invoke(echo);
}

void GameObserver::observeTimeOfDay(const MumeTimeEnum timeOfDay)
{
    if (m_timeOfDay != timeOfDay) {
        m_timeOfDay = timeOfDay;
        sig2_timeOfDayChanged.invoke(timeOfDay);
        emit timeOfDayChanged(timeOfDay);
    }
}

void GameObserver::observeMoonPhase(const MumeMoonPhaseEnum moonPhase)
{
    if (m_moonPhase != moonPhase) {
        m_moonPhase = moonPhase;
        sig2_moonPhaseChanged.invoke(moonPhase);
        emit moonPhaseChanged(moonPhase);
    }
}

void GameObserver::observeMoonVisibility(const MumeMoonVisibilityEnum moonVisibility)
{
    if (m_moonVisibility != moonVisibility) {
        m_moonVisibility = moonVisibility;
        sig2_moonVisibilityChanged.invoke(moonVisibility);
        emit moonVisibilityChanged(moonVisibility);
    }
}

void GameObserver::observeSeason(const MumeSeasonEnum season)
{
    if (m_season != season) {
        m_season = season;
        sig2_seasonChanged.invoke(season);
        emit seasonChanged(season);
    }
}

void GameObserver::observeWeather(const PromptWeatherEnum weather)
{
    if (m_weather != weather) {
        m_weather = weather;
        sig2_weatherChanged.invoke(weather);
    }
}

void GameObserver::observeFog(const PromptFogEnum fog)
{
    if (m_fog != fog) {
        m_fog = fog;
        sig2_fogChanged.invoke(fog);
    }
}

void GameObserver::observeCountdown(const QString &countdownText)
{
    if (m_countdownText != countdownText) {
        m_countdownText = countdownText;
        sig2_countdownChanged.invoke(countdownText);
        emit countdownChanged(countdownText);
    }
}

void GameObserver::observeTick(const MumeMoment &moment)
{
    sig2_tick.invoke(moment);
    emit tick(moment);
}
