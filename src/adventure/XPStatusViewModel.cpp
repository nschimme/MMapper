// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#include "XPStatusViewModel.h"
#include "AdventureTracker.h"
#include "adventuresession.h"
#include "../configuration/configuration.h"

XPStatusViewModel::XPStatusViewModel(AdventureTracker &tracker, QObject *parent)
    : QObject(parent), m_tracker(tracker)
{
    connect(&m_tracker, &AdventureTracker::sig_updatedSession, this, &XPStatusViewModel::updateSession);
    connect(&m_tracker, &AdventureTracker::sig_endedSession, this, &XPStatusViewModel::updateSession);

    updateContent();
}

void XPStatusViewModel::updateSession(const std::shared_ptr<AdventureSession> &session) {
    m_session = session;
    updateContent();
}

void XPStatusViewModel::updateContent() {
    bool shouldShow = getConfig().adventurePanel.getDisplayXPStatus() && m_session != nullptr;
    if (shouldShow) {
        auto xpSession = m_session->xp().gainedSession();
        auto tpSession = m_session->tp().gainedSession();
        auto xpf = AdventureSession::formatPoints(xpSession);
        auto tpf = AdventureSession::formatPoints(tpSession);
        m_text = QString("%1 Session: %2 XP %3 TP").arg(m_session->name(), xpf, tpf);
    } else {
        m_text = "";
    }

    if (m_visible != shouldShow) {
        m_visible = shouldShow;
        emit visibilityChanged();
    }
    emit textChanged();
}

void XPStatusViewModel::handleMouseEnter() {
    if (m_session) {
        auto xpHourly = m_session->calculateHourlyRateXP();
        auto tpHourly = m_session->calculateHourlyRateTP();
        auto xpf = AdventureSession::formatPoints(xpHourly);
        auto tpf = AdventureSession::formatPoints(tpHourly);
        emit showStatusMessage(QString("Hourly rate: %1 XP %2 TP").arg(xpf, tpf));
    }
}

void XPStatusViewModel::handleMouseLeave() {
    emit clearStatusMessage();
}
