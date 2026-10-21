// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2024 The MMapper Authors

#include "FindRoomTreeWidgetItem.h"

FindRoomTreeWidgetItem::~FindRoomTreeWidgetItem() = default;

NODISCARD bool FindRoomTreeWidgetItem::operator<(const QTreeWidgetItem &other) const
{
    const int col = treeWidget() ? treeWidget()->sortColumn() : 0;
    if (col == 0 || col == 2) {
        bool ok1 = false;
        bool ok2 = false;
        const double val1 = text(col).toDouble(&ok1);
        const double val2 = other.text(col).toDouble(&ok2);
        if (ok1 && ok2) {
            return val1 < val2;
        }
        if (ok1 != ok2) {
            // Numbers sort before non-numbers ("N/A")
            return ok1;
        }
    }
    return QTreeWidgetItem::operator<(other);
}
