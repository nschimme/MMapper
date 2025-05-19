// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2024 The MMapper Authors

#include "DescriptionWidget.h"

#include "../display/Filenames.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QPixmap>
#include <QResizeEvent>

DescriptionWidget::DescriptionWidget(QWidget *parent)
    : QWidget(parent)
    , m_label(new QLabel(this))
    , m_textEdit(new QTextEdit(this))
{
    m_label->setAlignment(Qt::AlignCenter);
    m_label->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    m_label->setGeometry(rect());

    m_textEdit->setGeometry(rect());
    m_textEdit->setReadOnly(true);
    m_textEdit->setFrameStyle(QFrame::NoFrame);
    m_textEdit->setVerticalScrollBarPolicy(Qt::ScrollBarPolicy::ScrollBarAsNeeded);
    m_textEdit->setHorizontalScrollBarPolicy(Qt::ScrollBarPolicy::ScrollBarAlwaysOff);

    m_textEdit->setAutoFillBackground(false);
    QPalette palette = m_textEdit->viewport()->palette();
    palette.setColor(QPalette::Base, QColor(0, 0, 0, 0));
    m_textEdit->viewport()->setPalette(palette);

    QPalette textPalette = m_textEdit->palette();
    textPalette.setColor(QPalette::Text, Qt::white);
    m_textEdit->setPalette(textPalette);
    m_textEdit->raise();

    setText("No room information available.");

    m_areaName = "valinor";
    updateLabel();
}

void DescriptionWidget::resizeEvent(QResizeEvent *event)
{
    const QSize &newSize = event->size();
    m_label->setGeometry(0, 0, newSize.width(), newSize.height());
    m_textEdit->setGeometry(0, 0, newSize.width(), newSize.height());
    updateLabel();
}

void DescriptionWidget::setText(const QString &text)
{
    m_textEdit->clear();

    // Set semi-transparent background for all text
    QTextBlockFormat blockFormat;
    blockFormat.setBackground(QColor(0, 0, 0, 160)); // semi-transparent black (alpha 160)

    // Apply format to existing text
    QTextCursor cursor = m_textEdit->textCursor();
    cursor.select(QTextCursor::Document);
    cursor.mergeBlockFormat(blockFormat);

    cursor.insertText(text);
}

void DescriptionWidget::updateLabel()
{
    if (m_areaName.isEmpty()) {
        return;
    }

    // Construct image path based on the stored area name
    QString imagePath;
    if (!m_areaName.isEmpty()) {
        imagePath = getResourceFilenameRaw("areas", m_areaName.toLower() + ".png");
    }

    // Set background image
    QPixmap pixmap(imagePath);
    if (!pixmap.isNull()) {
        m_label->setPixmap(
            pixmap.scaled(size(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation));
    }
}

void DescriptionWidget::slot_parseGmcpInput(const GmcpMessage &gmcp)
{
    if (gmcp.isRoomInfo()) {
        const auto &doc = gmcp.getJsonDocument();
        if (!doc) {
            return;
        }

        const OptJsonObj objOpt = doc->getObject();
        if (!objOpt) {
            return;
        }

        const JsonObj obj = *objOpt;

        // Update stored values if present in the GMCP message
        const std::optional<JsonString> roomNameOpt = obj.getString("name");
        if (roomNameOpt) {
            m_roomName = roomNameOpt.value();
        }

        const std::optional<JsonString> roomDescriptionOpt = obj.getString("desc");
        if (roomDescriptionOpt) {
            m_roomDescription = roomDescriptionOpt.value();
        }

        const std::optional<JsonString> areaNameOpt = obj.getString("area");
        if (areaNameOpt) {
            m_areaName = areaNameOpt.value();
            updateLabel();
        }

        QString text;
        if (!m_areaName.isEmpty()) {
            text += QString("%1\n").arg(m_areaName);
        }
        if (!m_roomName.isEmpty()) {
            text += QString("%1\n").arg(m_roomName);
        }
        if (!m_roomDescription.isEmpty()) {
            text += QString("%1").arg(m_roomDescription);
        }

        setText(text);
    }
}
