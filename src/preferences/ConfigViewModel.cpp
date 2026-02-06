// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#include "ConfigViewModel.h"

ConfigViewModel::ConfigViewModel(QObject *parent) : QObject(parent) {}
void ConfigViewModel::setCurrentPageIndex(int i) { if (m_currentPageIndex != i) { m_currentPageIndex = i; emit currentPageIndexChanged(); } }
