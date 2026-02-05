// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#include "MumeClockViewModel.h"

MumeClockViewModel::MumeClockViewModel(QObject *p) : QObject(p) {}

void MumeClockViewModel::update(const QString &time, const QString &season) {
    if (m_timeString != time || m_seasonString != season) {
        m_timeString = time;
        m_seasonString = season;
        emit clockChanged();
    }
}
