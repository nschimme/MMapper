// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#include "findreplacewidget.h"
#include "FindReplaceViewModel.h"
#include <QHBoxLayout>
#include <QToolButton>
#include <QKeyEvent>

FindReplaceWidget::FindReplaceWidget(bool allowReplace, QWidget *parent)
    : QWidget(parent)
{
    auto *viewModel = new FindReplaceViewModel(allowReplace, this);

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(5, 2, 5, 2);

    m_findLineEdit = new QLineEdit(this);
    m_findLineEdit->setPlaceholderText(tr("Find..."));
    layout->addWidget(m_findLineEdit);

    m_findNextButton = new QToolButton(this);
    m_findNextButton->setText(tr("Next"));
    layout->addWidget(m_findNextButton);

    connect(m_findLineEdit, &QLineEdit::textChanged, viewModel, &FindReplaceViewModel::setFindText);
    connect(m_findNextButton, &QToolButton::clicked, viewModel, &FindReplaceViewModel::findNext);

    connect(viewModel, &FindReplaceViewModel::sig_findRequested, this, &FindReplaceWidget::sig_findRequested);

    if (allowReplace) {
        m_replaceLineEdit = new QLineEdit(this);
        m_replaceLineEdit->setPlaceholderText(tr("Replace with..."));
        layout->addWidget(m_replaceLineEdit);

        m_replaceCurrentButton = new QToolButton(this);
        m_replaceCurrentButton->setText(tr("Replace"));
        layout->addWidget(m_replaceCurrentButton);

        connect(m_replaceLineEdit, &QLineEdit::textChanged, viewModel, &FindReplaceViewModel::setReplaceText);
        connect(m_replaceCurrentButton, &QToolButton::clicked, viewModel, &FindReplaceViewModel::replaceCurrent);

        connect(viewModel, &FindReplaceViewModel::sig_replaceCurrentRequested, this, &FindReplaceWidget::sig_replaceCurrentRequested);
    }
}

FindReplaceWidget::~FindReplaceWidget() = default;

void FindReplaceWidget::setFocusToFindInput() {
    m_findLineEdit->setFocus();
    m_findLineEdit->selectAll();
}

void FindReplaceWidget::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Escape) {
        emit sig_closeRequested();
    }
    QWidget::keyPressEvent(event);
}

void FindReplaceWidget::slot_updateButtonStates() {}
QToolButton *FindReplaceWidget::createActionButton(const QString &, const QString &, const QString &, const QString &, bool, Qt::ToolButtonStyle, Qt::FocusPolicy) { return nullptr; }
