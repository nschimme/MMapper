// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2024 The MMapper Authors

#include "groupproxymodel.h"
#include "groupwidget.h" // Contains GroupModel definition for now
#include "CGroupChar.h"
#include "../configuration/configuration.h" // For getConfig()
#include "../global/common.h" // For Q_UNUSED if not available otherwise, or remove if Q_UNUSED is std

GroupProxyModel::GroupProxyModel(QObject *parent)
    : QSortFilterProxyModel(parent)
{
}

GroupProxyModel::~GroupProxyModel() = default;

void GroupProxyModel::refresh()
{
    invalidate(); // Triggers re-filter and re-sort
}

SharedGroupChar GroupProxyModel::getCharacterFromSource(const QModelIndex &source_index) const
{
    const GroupModel *srcModel = qobject_cast<const GroupModel*>(sourceModel());
    if (!srcModel || !source_index.isValid()) {
        return nullptr;
    }

    Mmapper2Group* group = srcModel->getGroup();
    if (!group) {
        return nullptr;
    }

    // Assuming GroupModel directly uses Mmapper2Group's selectAll() for its rows
    // The source_index.row() directly maps to the index in this unfiltered, unsorted list
    GroupVector characters = group->selectAll();
    if (source_index.row() >= 0 && source_index.row() < static_cast<int>(characters.size())) {
        return characters.at(static_cast<size_t>(source_index.row()));
    }
    return nullptr;
}

bool GroupProxyModel::filterAcceptsRow(int source_row, const QModelIndex &source_parent) const
{
    Q_UNUSED(source_parent); // Typically not used for flat list models

    const auto& groupManagerSettings = getConfig().groupManager;
    if (!groupManagerSettings.filterNPCs) {
        return true; // If filtering is off, accept all rows
    }

    // Get the character for the given source_row from the source model
    QModelIndex sourceIndex = sourceModel()->index(source_row, 0, source_parent);
    SharedGroupChar character = getCharacterFromSource(sourceIndex);

    if (character && character->isNPC()) {
        return false; // Filter out (hide) NPCs
    }

    return true; // Accept non-NPCs or if character is null (should ideally not happen for valid rows)
}

bool GroupProxyModel::lessThan(const QModelIndex &source_left, const QModelIndex &source_right) const
{
    SharedGroupChar leftChar = getCharacterFromSource(source_left);
    SharedGroupChar rightChar = getCharacterFromSource(source_right);

    if (!leftChar || !rightChar) {
        // Fall back to default comparison if characters can't be retrieved.
        return QSortFilterProxyModel::lessThan(source_left, source_right);
    }

    const auto& groupManagerSettings = getConfig().groupManager;

    if (groupManagerSettings.sortNpcsToBottom) {
        bool leftIsNpc = leftChar->isNPC();
        bool rightIsNpc = rightChar->isNPC();

        if (leftIsNpc && !rightIsNpc) {
            return false; // NPCs (left) are "greater than" non-NPCs (right), so they come after.
        }
        if (!leftIsNpc && rightIsNpc) {
            return true;  // Non-NPCs (left) are "less than" NPCs (right), so they come before.
        }
        // If both are NPCs or both are non-NPCs, they are considered "equal" by this rule.
        // Fall through to return false to preserve original order.
    }

    // If sortNpcsToBottom is OFF, or if characters are of the same type according to the rule above:
    // Return false. This indicates that 'left' is not strictly less than 'right' based on
    // our custom criteria. For a stable sort, QSortFilterProxyModel will then preserve
    // their relative order from the source model.
    return false;
}
