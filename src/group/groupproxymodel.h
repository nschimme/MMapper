#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2024 The MMapper Authors

#include <QSortFilterProxyModel>
#include "CGroupChar.h" // For SharedGroupChar

class NODISCARD_QOBJECT GroupProxyModel final : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    explicit GroupProxyModel(QObject *parent = nullptr);
    ~GroupProxyModel() final;

    void refresh(); // Method to trigger re-evaluation

protected:
    bool filterAcceptsRow(int source_row, const QModelIndex &source_parent) const override;
    bool lessThan(const QModelIndex &source_left, const QModelIndex &source_right) const override;

private:
    // Helper to get character from source model - to be fully implemented later
    SharedGroupChar getCharacterFromSource(const QModelIndex &source_index) const;
};
