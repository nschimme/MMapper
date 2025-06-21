// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2024 The MMapper Authors

#include "ChangeList.h"

#include <algorithm> // For std::copy / std::back_inserter if needed, or range-based for loop

void ChangeList::append(const ChangeList& other)
{
    m_changes.insert(m_changes.end(), other.m_changes.begin(), other.m_changes.end());
}
