#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#include "../global/macros.h"
#include <QObject>

class NODISCARD_QOBJECT GotoViewModel final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int lineNum READ lineNum WRITE setLineNum NOTIFY lineNumChanged)

public:
    explicit GotoViewModel(QObject *parent = nullptr);

    NODISCARD int lineNum() const { return m_lineNum; }
    void setLineNum(int l);

    void requestGoto();

signals:
    void lineNumChanged();
    void sig_gotoLineRequested(int lineNum);

private:
    int m_lineNum = 1;
};
