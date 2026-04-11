// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2024 The MMapper Authors

#include "TimerWidget.h"

#include "CTimers.h"
#include "TimerDelegate.h"
#include "TimerModel.h"

#include <QHeaderView>
#include <QMenu>
#include <QPushButton>
#include <QVBoxLayout>

TimerWidget::TimerWidget(CTimers &timers, QWidget *parent)
    : QWidget(parent)
    , m_timers(timers)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_model = new TimerModel(m_timers, this);
    m_view = new QTableView(this);
    m_view->setModel(m_model);
    m_view->setItemDelegateForColumn(TimerModel::ColName, new TimerDelegate(this));
    m_view->horizontalHeader()->setStretchLastSection(true);
    m_view->verticalHeader()->setVisible(false);
    m_view->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_view->setContextMenuPolicy(Qt::CustomContextMenu);
    m_view->setShowGrid(false);
    m_view->setAlternatingRowColors(true);

    layout->addWidget(m_view);

    auto *btnLayout = new QHBoxLayout();
    auto *btnClear = new QPushButton(tr("Clear Expired"), this);
    connect(btnClear, &QPushButton::clicked, this, &TimerWidget::clearExpired);
    btnLayout->addStretch();
    btnLayout->addWidget(btnClear);
    layout->addLayout(btnLayout);

    connect(m_view, &QTableView::customContextMenuRequested, this, &TimerWidget::showContextMenu);
}

void TimerWidget::showContextMenu(const QPoint &pos)
{
    QModelIndex index = m_view->indexAt(pos);
    if (!index.isValid())
        return;

    const TTimer *timer = m_model->timerAt(index.row());
    if (!timer)
        return;

    QMenu menu(this);
    std::string name = timer->getName();
    bool isCountdown = timer->isCountdown();

    auto *actReset = menu.addAction(tr("Reset"));
    connect(actReset, &QAction::triggered, this, [this, name, isCountdown]() {
        if (isCountdown)
            m_timers.resetCountdown(name);
        else
            m_timers.resetTimer(name);
    });

    auto *actStop = menu.addAction(tr("Stop"));
    actStop->setEnabled(!timer->isExpired());
    connect(actStop, &QAction::triggered, this, [this, name, isCountdown]() {
        if (isCountdown)
            m_timers.stopCountdown(name);
        else
            m_timers.stopTimer(name);
    });

    menu.addSeparator();

    auto *actDelete = menu.addAction(tr("Delete"));
    connect(actDelete, &QAction::triggered, this, [this, name, isCountdown]() {
        if (isCountdown)
            std::ignore = m_timers.removeCountdown(name);
        else
            std::ignore = m_timers.removeTimer(name);
    });

    menu.exec(m_view->viewport()->mapToGlobal(pos));
}

void TimerWidget::clearExpired()
{
    m_timers.clearExpired();
}
