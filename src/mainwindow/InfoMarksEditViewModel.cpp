// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#include "InfoMarksEditViewModel.h"

InfoMarksEditViewModel::InfoMarksEditViewModel(QObject *p) : QObject(p) {}
void InfoMarksEditViewModel::setCurrentIndex(int i) { if (m_currentIndex != i) { m_currentIndex = i; emit currentIndexChanged(); } }
