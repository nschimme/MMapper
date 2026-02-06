// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#include "RemoteEditViewModel.h"

RemoteEditViewModel::RemoteEditViewModel(bool editSession, const QString &title, const QString &body, QObject *parent)
    : QObject(parent), m_editSession(editSession), m_title(title), m_text(body) {}

void RemoteEditViewModel::setText(const QString &t) { if (m_text != t) { m_text = t; emit textChanged(); } }
void RemoteEditViewModel::submit() { emit sig_save(m_text); }
void RemoteEditViewModel::cancel() { emit sig_cancel(); }
