#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2023 The MMapper Authors
// Author: Mike Repass <mike.repass@gmail.com> (Taryn)

#include "../clock/mumemoment.h"
#include "../global/ChangeMonitor.h"
#include "../global/Signal2.h"
#include "../map/PromptFlags.h"
#include "../proxy/GmcpMessage.h"

#include <QObject>

class NODISCARD_QOBJECT GameObserver final
{
private:
    MumeTimeEnum m_timeOfDay = MumeTimeEnum::UNKNOWN;
    MumeMoonPhaseEnum m_moonPhase = MumeMoonPhaseEnum::UNKNOWN;
    MumeMoonVisibilityEnum m_moonVisibility = MumeMoonVisibilityEnum::UNKNOWN;
    PromptWeatherEnum m_weather = PromptWeatherEnum::NICE;
    PromptFogEnum m_fog = PromptFogEnum::NO_FOG;

public:
    Signal2<> sig2_connected;

    Signal2<QString> sig2_sentToMudString;  // removes ANSI
    Signal2<QString> sig2_sentToUserString; // removes ANSI

    Signal2<GmcpMessage> sig2_sentToUserGmcp;
    Signal2<bool> sig2_toggledEchoMode;

    Signal2<MumeTimeEnum> sig2_timeOfDayChanged;
    Signal2<MumeMoonPhaseEnum> sig2_moonPhaseChanged;
    Signal2<MumeMoonVisibilityEnum> sig2_moonVisibilityChanged;
    Signal2<PromptWeatherEnum> sig2_weatherChanged;
    Signal2<PromptFogEnum> sig2_fogChanged;

public:
    void observeConnected();
    void observeSentToMud(const QString &ba);
    void observeSentToUser(const QString &ba);
    void observeSentToUserGmcp(const GmcpMessage &m);
    void observeToggledEchoMode(bool echo);

    void observeTimeOfDay(MumeTimeEnum timeOfDay);
    void observeMoonPhase(MumeMoonPhaseEnum moonPhase);
    void observeMoonVisibility(MumeMoonVisibilityEnum moonVisibility);
    void observeWeather(PromptWeatherEnum weather);
    void observeFog(PromptFogEnum fog);

    MumeTimeEnum getTimeOfDay() const { return m_timeOfDay; }
    MumeMoonPhaseEnum getMoonPhase() const { return m_moonPhase; }
    MumeMoonVisibilityEnum getMoonVisibility() const { return m_moonVisibility; }
    PromptWeatherEnum getWeather() const { return m_weather; }
    PromptFogEnum getFog() const { return m_fog; }
};
