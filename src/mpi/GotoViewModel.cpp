// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#include "GotoViewModel.h"

GotoViewModel::GotoViewModel(QObject *parent) : QObject(parent) {}
void GotoViewModel::setLineNum(int l) { if (m_lineNum != l) { m_lineNum = l; emit lineNumChanged(); } }
void GotoViewModel::requestGoto() { emit sig_gotoLineRequested(m_lineNum); }
