#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#include "../global/macros.h"
#include <QObject>

class NODISCARD_QOBJECT ConfigViewModel final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int currentPageIndex READ currentPageIndex WRITE setCurrentPageIndex NOTIFY currentPageIndexChanged)

public:
    explicit ConfigViewModel(QObject *parent = nullptr);

    NODISCARD int currentPageIndex() const { return m_currentPageIndex; }
    void setCurrentPageIndex(int i);

signals:
    void currentPageIndexChanged();

private:
    int m_currentPageIndex = 0;
};
