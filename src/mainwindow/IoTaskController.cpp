// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "IoTaskController.h"

IoTaskController::IoTaskController(QObject *const parent)
    : QObject(parent)
{}

IoTaskController::~IoTaskController() = default;

void IoTaskController::begin(const QString &label, const bool cancelable)
{
    m_active = true;
    m_label = label;
    m_percent = 0;
    m_cancelable = cancelable;
    emit sig_changed();
}

void IoTaskController::update(const QString &label, const int percent)
{
    if (!m_active) {
        return;
    }
    m_label = label;
    m_percent = percent;
    emit sig_changed();
}

void IoTaskController::end()
{
    if (!m_active) {
        return;
    }
    m_active = false;
    m_label.clear();
    m_percent = 0;
    m_cancelable = false;
    emit sig_changed();
}

void IoTaskController::cancel()
{
    if (m_cancelable) {
        emit sig_cancelRequested();
    }
}
