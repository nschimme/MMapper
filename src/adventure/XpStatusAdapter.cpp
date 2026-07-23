// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "XpStatusAdapter.h"

#include "../configuration/configuration.h"
#include "adventuresession.h"
#include "adventuretracker.h"

#include <memory>

XpStatusAdapter::XpStatusAdapter(AdventureTracker &at, QObject *const parent)
    : QObject(parent)
    , m_tracker{at}
{
    updateContent();

    connect(&m_tracker,
            &AdventureTracker::sig_updatedSession,
            this,
            &XpStatusAdapter::slot_updatedSession);

    connect(&m_tracker,
            &AdventureTracker::sig_endedSession,
            this,
            &XpStatusAdapter::slot_updatedSession);

    setConfig().adventurePanel.registerChangeCallback(m_lifetime, [this]() { updateContent(); });
}

void XpStatusAdapter::updateContent()
{
    if (getConfig().adventurePanel.getDisplayXPStatus() && m_session != nullptr) {
        auto xpSession = m_session->xp().gainedSession();
        auto tpSession = m_session->tp().gainedSession();
        auto xpf = AdventureSession::formatPoints(xpSession);
        auto tpf = AdventureSession::formatPoints(tpSession);
        m_text = QString("%1 Session: %2 XP %3 TP").arg(m_session->name(), xpf, tpf);
        m_shown = true;
    } else {
        m_text.clear();
        m_shown = false;
    }
    emit sig_changed();
}

void XpStatusAdapter::slot_updatedSession(const std::shared_ptr<AdventureSession> &session)
{
    m_session = session;
    updateContent();
}

QString XpStatusAdapter::hourlyRateText() const
{
    if (m_session == nullptr) {
        return {};
    }
    auto xpHourly = m_session->calculateHourlyRateXP();
    auto tpHourly = m_session->calculateHourlyRateTP();
    auto xpf = AdventureSession::formatPoints(xpHourly);
    auto tpf = AdventureSession::formatPoints(tpHourly);
    return QString("Hourly rate: %1 XP %2 TP").arg(xpf, tpf);
}

void XpStatusAdapter::hoverEntered()
{
    const auto msg = hourlyRateText();
    if (!msg.isEmpty()) {
        emit sig_showStatusMessage(msg);
    }
}

void XpStatusAdapter::hoverExited()
{
    emit sig_clearStatusMessage();
}

void XpStatusAdapter::clicked()
{
    emit sig_toggleAdventurePanel();
}
