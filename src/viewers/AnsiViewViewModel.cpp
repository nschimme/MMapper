// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#include "AnsiViewViewModel.h"

AnsiViewViewModel::AnsiViewViewModel(QObject *p) : QObject(p) {}

void AnsiViewViewModel::setText(const QString &t) {
    if (m_text != t) { m_text = t; emit textChanged(); }
}
