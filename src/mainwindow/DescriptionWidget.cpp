// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#include "DescriptionWidget.h"
#include "../configuration/configuration.h"
#include <QPainter>
#include <QResizeEvent>
#include <QFontMetrics>
#include <QTextBlockFormat>
#include <QTextCursor>

DescriptionWidget::DescriptionWidget(QWidget *parent)
    : QWidget(parent)
    , m_label(new QLabel(this))
    , m_textEdit(new QTextEdit(this))
{
    m_label->setAlignment(Qt::AlignCenter);
    m_textEdit->setReadOnly(true);
    m_textEdit->setFrameStyle(QFrame::NoFrame);

    // Transparent background for text edit to see the blurred image behind it
    m_textEdit->setAutoFillBackground(false);
    QPalette palette = m_textEdit->viewport()->palette();
    palette.setColor(QPalette::Base, QColor(0, 0, 0, 0));
    m_textEdit->viewport()->setPalette(palette);
    m_textEdit->raise();

    connect(&m_viewModel, &DescriptionViewModel::roomNameChanged, this, &DescriptionWidget::updateUI);
    connect(&m_viewModel, &DescriptionViewModel::roomDescriptionChanged, this, &DescriptionWidget::updateUI);
    connect(&m_viewModel, &DescriptionViewModel::blurredImageChanged, this, &DescriptionWidget::updateBackground);

    updateUI();
}

void DescriptionWidget::updateUI() {
    m_textEdit->clear();

    // Set colors and partial background
    QTextBlockFormat blockFormat;
    blockFormat.setBackground(m_viewModel.backgroundColor());
    QTextCursor cursor = m_textEdit->textCursor();
    cursor.select(QTextCursor::Document);
    cursor.mergeBlockFormat(blockFormat);

    QTextCharFormat nameFormat;
    nameFormat.setForeground(m_viewModel.roomNameColor());
    cursor.insertText(m_viewModel.roomName() + "\n", nameFormat);

    QTextCharFormat descFormat;
    descFormat.setForeground(m_viewModel.roomDescColor());
    cursor.insertText(m_viewModel.roomDescription(), descFormat);
}

void DescriptionWidget::updateBackground() {
    QImage img = m_viewModel.blurredImage();
    if (img.isNull()) {
        m_label->clear();
        return;
    }
    m_label->setPixmap(QPixmap::fromImage(img));
}

void DescriptionWidget::resizeEvent(QResizeEvent *event) {
    m_label->setGeometry(rect());
    m_textEdit->setGeometry(rect());
    m_viewModel.setWidgetSize(size());
}

QSize DescriptionWidget::minimumSizeHint() const { return QSize(100, 100); }
QSize DescriptionWidget::sizeHint() const { return QSize(400, 600); }
