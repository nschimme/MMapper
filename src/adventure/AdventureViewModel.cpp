// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#include "AdventureViewModel.h"
#include "adventuretracker.h"

AdventureViewModel::AdventureViewModel(AdventureTracker &tracker, QObject *parent)
    : QObject(parent), m_tracker(tracker)
{
    connect(&m_tracker, &AdventureTracker::sig_accomplishedTask, this, &AdventureViewModel::onAccomplishedTask);
    connect(&m_tracker, &AdventureTracker::sig_achievedSomething, this, &AdventureViewModel::onAchievedSomething);
    connect(&m_tracker, &AdventureTracker::sig_diedInGame, this, &AdventureViewModel::onDied);
    connect(&m_tracker, &AdventureTracker::sig_gainedLevel, this, &AdventureViewModel::onGainedLevel);
    connect(&m_tracker, &AdventureTracker::sig_killedMob, this, &AdventureViewModel::onKilledMob);
    connect(&m_tracker, &AdventureTracker::sig_receivedHint, this, &AdventureViewModel::onReceivedHint);
}

void AdventureViewModel::clear() {
    m_messages.clear();
    emit messagesChanged();
}

void AdventureViewModel::addMessage(const QString &msg) {
    m_messages.append(msg);
    if (m_messages.size() > 1024) m_messages.removeFirst();
    emit sig_messageAdded(msg);
    emit messagesChanged();
}

void AdventureViewModel::onAccomplishedTask(double xpGained) {
    addMessage(QString("Task accomplished! (%1 xp)").arg(xpGained));
}

void AdventureViewModel::onAchievedSomething(const QString &achievement, double xpGained) {
    if (xpGained > 0) addMessage(QString("Achievement: %1 (%2 xp)").arg(achievement).arg(xpGained));
    else addMessage(QString("Achievement: %1").arg(achievement));
}

void AdventureViewModel::onDied(double xpLost) {
    addMessage(QString("You are dead! Sorry... (%1 xp)").arg(xpLost));
}

void AdventureViewModel::onGainedLevel() {
    addMessage("You gain a level! Congrats!");
}

void AdventureViewModel::onKilledMob(const QString &mobName, double xpGained) {
    addMessage(QString("Trophy: %1 (%2 xp)").arg(mobName).arg(xpGained));
}

void AdventureViewModel::onReceivedHint(const QString &hint) {
    addMessage(QString("Hint: %1").arg(hint));
}
