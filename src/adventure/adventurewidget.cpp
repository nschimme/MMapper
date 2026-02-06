// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2023 The MMapper Authors

#include "adventurewidget.h"
#include "../configuration/configuration.h"
#include <QVBoxLayout>
#include <QMenu>
#include <QScrollBar>

AdventureWidget::AdventureWidget(AdventureTracker &at, QWidget *parent)
    : QWidget(parent)
{
    m_viewModel = std::make_unique<AdventureViewModel>(at, this);

    m_textEdit = new QTextEdit(this);
    m_textEdit->setReadOnly(true);

    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_textEdit);

    connect(m_viewModel.get(), &AdventureViewModel::sig_messageAdded, this, &AdventureWidget::updateUI);

    m_clearContentAction = new QAction("Clear Content", this);
    connect(m_clearContentAction, &QAction::triggered, m_viewModel.get(), &AdventureViewModel::clear);
    connect(m_viewModel.get(), &AdventureViewModel::messagesChanged, [this]() {
        if (m_viewModel->messages().isEmpty()) m_textEdit->clear();
    });

    m_textEdit->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_textEdit, &QTextEdit::customContextMenuRequested, this, &AdventureWidget::slot_contextMenuRequested);

    // Initial content
    for (const auto &msg : m_viewModel->messages()) updateUI(msg);
}

void AdventureWidget::updateUI(const QString &msg) {
    m_textEdit->append(msg);
    m_textEdit->verticalScrollBar()->setValue(m_textEdit->verticalScrollBar()->maximum());
}

void AdventureWidget::slot_contextMenuRequested(const QPoint &pos) {
    std::unique_ptr<QMenu> contextMenu(m_textEdit->createStandardContextMenu());
    contextMenu->addSeparator();
    contextMenu->addAction(m_clearContentAction);
    contextMenu->exec(m_textEdit->mapToGlobal(pos));
}
