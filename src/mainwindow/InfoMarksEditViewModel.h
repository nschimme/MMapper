#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#include "../global/macros.h"
#include <QObject>
#include <QStringList>

class NODISCARD_QOBJECT InfoMarksEditViewModel final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QStringList objectNames READ objectNames NOTIFY objectNamesChanged)
    Q_PROPERTY(int currentIndex READ currentIndex WRITE setCurrentIndex NOTIFY currentIndexChanged)

public:
    explicit InfoMarksEditViewModel(QObject *parent = nullptr);

    NODISCARD QStringList objectNames() const { return m_objectNames; }
    NODISCARD int currentIndex() const { return m_currentIndex; }
    void setCurrentIndex(int i);

signals:
    void objectNamesChanged();
    void currentIndexChanged();

private:
    QStringList m_objectNames;
    int m_currentIndex = -1;
};
