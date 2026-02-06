// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#include "gotowidget.h"
#include "GotoViewModel.h"
#include <QHBoxLayout>
#include <QLabel>
#include <QIntValidator>
#include <QKeyEvent>

GotoWidget::GotoWidget(QWidget *parent)
    : QWidget(parent)
{
    auto *viewModel = new GotoViewModel(this);

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(5, 2, 5, 2);

    layout->addWidget(new QLabel(tr("Go to line:"), this));

    m_lineEdit = new QLineEdit(this);
    m_lineEdit->setValidator(new QIntValidator(1, 999999, this));
    layout->addWidget(m_lineEdit);

    connect(m_lineEdit, &QLineEdit::textChanged, [viewModel](const QString &t) {
        viewModel->setLineNum(t.toInt());
    });

    connect(viewModel, &GotoViewModel::sig_gotoLineRequested, this, &GotoWidget::sig_gotoLineRequested);

    m_lineEdit->installEventFilter(this);
}

GotoWidget::~GotoWidget() = default;

void GotoWidget::setFocusToInput() {
    m_lineEdit->setFocus();
    m_lineEdit->selectAll();
}

void GotoWidget::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        // ViewModel request goto
        emit sig_gotoLineRequested(m_lineEdit->text().toInt());
    } else if (event->key() == Qt::Key_Escape) {
        emit sig_closeRequested();
    }
    QWidget::keyPressEvent(event);
}
