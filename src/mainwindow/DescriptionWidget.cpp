// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#include "DescriptionWidget.h"
#include "../configuration/configuration.h"
#include <QPainter>
#include <QResizeEvent>
#include <QFontMetrics>

DescriptionWidget::DescriptionWidget(QWidget *parent)
    : QWidget(parent)
    , m_label(new QLabel(this))
    , m_textEdit(new QTextEdit(this))
{
    m_label->setAlignment(Qt::AlignCenter);
    m_textEdit->setReadOnly(true);
    m_textEdit->setFrameStyle(QFrame::NoFrame);

    connect(&m_viewModel, &DescriptionViewModel::roomNameChanged, this, &DescriptionWidget::updateUI);
    connect(&m_viewModel, &DescriptionViewModel::roomDescriptionChanged, this, &DescriptionWidget::updateUI);
    connect(&m_viewModel, &DescriptionViewModel::backgroundImagePathChanged, this, &DescriptionWidget::updateBackground);

    updateUI();
}

void DescriptionWidget::updateUI() {
    m_textEdit->clear();
    m_textEdit->append(m_viewModel.roomName());
    m_textEdit->append(m_viewModel.roomDescription());
}

void DescriptionWidget::updateBackground() {
    QString path = m_viewModel.backgroundImagePath();
    if (path.isEmpty()) {
        m_label->clear();
        return;
    }
    QImage img(path);
    m_label->setPixmap(QPixmap::fromImage(img).scaled(size(), Qt::KeepAspectRatio));
}

void DescriptionWidget::resizeEvent(QResizeEvent *event) {
    m_label->setGeometry(rect());
    m_textEdit->setGeometry(rect());
    updateBackground();
}

QSize DescriptionWidget::minimumSizeHint() const { return QSize(100, 100); }
QSize DescriptionWidget::sizeHint() const { return QSize(400, 600); }
