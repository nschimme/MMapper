// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#include "AnsiColorViewModel.h"

AnsiColorViewModel::AnsiColorViewModel(QObject *p) : QObject(p) {}
void AnsiColorViewModel::setAnsiString(const QString &v) { if (m_ansiString != v) { m_ansiString = v; emit ansiStringChanged(); } }
