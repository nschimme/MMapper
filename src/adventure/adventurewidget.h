#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2023 The MMapper Authors

#include "AdventureViewModel.h"
#include <QWidget>
#include <QTextEdit>
#include <memory>

class AdventureTracker;

class NODISCARD_QOBJECT AdventureWidget : public QWidget
{
    Q_OBJECT
private:
    std::unique_ptr<AdventureViewModel> m_viewModel;
    QTextEdit *m_textEdit = nullptr;
    QAction *m_clearContentAction = nullptr;

public:
    explicit AdventureWidget(AdventureTracker &at, QWidget *parent);
    ~AdventureWidget() final = default;

private slots:
    void updateUI(const QString &msg);
    void slot_contextMenuRequested(const QPoint &pos);
};
