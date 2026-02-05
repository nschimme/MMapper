// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#include "PasswordViewModel.h"

PasswordViewModel::PasswordViewModel(QObject *parent) : QObject(parent) {}
void PasswordViewModel::submitPassword(const QString &p) { emit sig_passwordSubmitted(p); }
