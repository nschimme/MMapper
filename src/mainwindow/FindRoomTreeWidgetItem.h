#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2024 The MMapper Authors

#include "../global/macros.h"

#include <QtWidgets/QTreeWidget>
#include <QtWidgets/QTreeWidgetItem>

class NODISCARD FindRoomTreeWidgetItem final : public QTreeWidgetItem
{
public:
    using QTreeWidgetItem::QTreeWidgetItem;
    ~FindRoomTreeWidgetItem() override;

    NODISCARD bool operator<(const QTreeWidgetItem &other) const override;
};
