// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "remoteeditwidget.h"
#include "gotowidget.h"
#include "findreplacewidget.h"
#include <QVBoxLayout>
#include <QMenuBar>
#include <QStatusBar>

RemoteTextEdit::RemoteTextEdit(const QString &text, QWidget *parent) : QPlainTextEdit(parent) { setPlainText(text); }
void RemoteTextEdit::replaceAll(const QString &text) { setPlainText(text); }

RemoteEditWidget::RemoteEditWidget(bool editSession, QString title, QString body, QWidget *parent)
    : QDialog(parent)
{
    m_viewModel = std::make_unique<RemoteEditViewModel>(editSession, title, body, this);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    m_menuBar = new QMenuBar(this);
    layout->setMenuBar(m_menuBar);

    m_gotoWidget = new GotoWidget(this);
    m_gotoWidget->hide();
    layout->addWidget(m_gotoWidget);

    m_findReplaceWidget = new FindReplaceWidget(editSession, this);
    m_findReplaceWidget->hide();
    layout->addWidget(m_findReplaceWidget);

    m_textEdit = new RemoteTextEdit(body, this);
    layout->addWidget(m_textEdit);

    m_statusBar = new QStatusBar(this);
    layout->addWidget(m_statusBar);

    connect(m_viewModel.get(), &RemoteEditViewModel::sig_save, this, &RemoteEditWidget::slot_save);
    connect(m_viewModel.get(), &RemoteEditViewModel::sig_cancel, this, &RemoteEditWidget::slot_cancel);

    connect(m_textEdit, &QPlainTextEdit::textChanged, [this]() {
        m_viewModel->setText(m_textEdit->toPlainText());
    });
}

RemoteEditWidget::~RemoteEditWidget() = default;

void RemoteEditWidget::slot_save(const QString &text) { emit sig_save(text); close(); }
void RemoteEditWidget::slot_cancel() { emit sig_cancel(); close(); }
