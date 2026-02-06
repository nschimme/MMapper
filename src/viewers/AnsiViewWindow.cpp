// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2024 The MMapper Authors

#include "AnsiViewWindow.h"
#include "../client/displaywidget.h"
#include <QTextBrowser>
#include <QVBoxLayout>
#include <QApplication>
#include <QScreen>

AnsiViewWindow::AnsiViewWindow(const QString &program, const QString &title, std::string_view message)
    : QDialog(nullptr), m_view(std::make_unique<QTextBrowser>(this))
{
    setWindowTitle(QString("%1 - %2").arg(program, title));
    QVBoxLayout *l = new QVBoxLayout(this);
    l->addWidget(m_view.get());

    connect(&m_viewModel, &AnsiViewViewModel::textChanged, this, &AnsiViewWindow::updateUI);
    m_viewModel.setText(QString::fromUtf8(message.data(), message.size()));

    setGeometry(QStyle::alignedRect(Qt::LeftToRight, Qt::AlignCenter, size(), qApp->primaryScreen()->availableGeometry()));
}

AnsiViewWindow::~AnsiViewWindow() = default;

void AnsiViewWindow::updateUI() {
    setAnsiText(m_view.get(), m_viewModel.text().toStdString());
}

std::unique_ptr<AnsiViewWindow> makeAnsiViewWindow(const QString &program, const QString &title, std::string_view body) {
    return std::make_unique<AnsiViewWindow>(program, title, body);
}
