// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "AudioHintWidget.h"

#include "../configuration/configuration.h"
#include "../media/AudioManager.h"

#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QVBoxLayout>

AudioHintWidget::AudioHintWidget(AudioManager &audioManager, QWidget *parent)
    : QWidget(parent)
    , m_audioManager(audioManager)
{
    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setContentsMargins(10, 2, 10, 2);
    layout->setSpacing(8);

    m_iconLabel = new QLabel(this);
    m_iconLabel->setPixmap(QIcon(":/icons/audio.png").pixmap(16, 16));
    layout->addWidget(m_iconLabel);

    m_textLabel = new QLabel(tr("Play with audio?"), this);
    m_textLabel->setStyleSheet("font-weight: bold; color: white;");
    layout->addWidget(m_textLabel);

    m_yesButton = new QPushButton(tr("Yes"), this);
    m_yesButton->setFixedWidth(50);
    m_yesButton->setCursor(Qt::PointingHandCursor);
    m_yesButton->setStyleSheet("QPushButton { background-color: #4a90e2; color: white; border: "
                               "none; border-radius: 3px; padding: 2px; }"
                               "QPushButton:hover { background-color: #357abd; }");

    m_noButton = new QPushButton(tr("No"), this);
    m_noButton->setFixedWidth(50);
    m_noButton->setCursor(Qt::PointingHandCursor);
    m_noButton->setStyleSheet("QPushButton { background-color: #666; color: white; border: none; "
                              "border-radius: 3px; padding: 2px; }"
                              "QPushButton:hover { background-color: #555; }");

    layout->addWidget(m_yesButton);
    layout->addWidget(m_noButton);
    layout->addStretch();

    connect(m_yesButton, &QPushButton::clicked, this, [this]() {
        m_audioManager.unblockAudio();
        this->hide();
    });

    connect(m_noButton, &QPushButton::clicked, this, [this]() {
        // Set volumes to 0 if they choose "No"
        setConfig().audio.setMusicVolume(0);
        setConfig().audio.setSoundVolume(0);
        this->hide();
    });

    setStyleSheet("background-color: #333; border-bottom: 1px solid #555;");
    setFixedHeight(30);

    connect(&m_audioManager, &AudioManager::sig_audioUnblocked, this, &AudioHintWidget::hide);

    // Hint is always shown on startup/creation for Wasm as per request
    setVisible(true);
}

AudioHintWidget::~AudioHintWidget() = default;
