// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "AudioHint.h"

#include "../configuration/configuration.h"
#include "AudioManager.h"

#include <QIcon>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QVBoxLayout>

AudioHint::AudioHint(AudioManager &audioManager, QWidget *parent)
    : QWidget(parent)
    , m_audioManager(audioManager)
{
    // Semi-transparent background covering the whole parent
    setAttribute(Qt::WA_DeleteOnClose);

    m_container = new QWidget(this);
    m_container->setObjectName("hintContainer");
    m_container->setStyleSheet("QWidget#hintContainer {"
                               "  background-color: #333333;"
                               "  border: 2px solid #555555;"
                               "  border-radius: 10px;"
                               "}"
                               "QLabel {"
                               "  color: white;"
                               "  font-size: 14px;"
                               "}");

    QVBoxLayout *layout = new QVBoxLayout(m_container);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(15);

    m_iconLabel = new QLabel(m_container);
    m_iconLabel->setPixmap(QIcon(":/icons/audio.png").pixmap(64, 64));
    m_iconLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_iconLabel);

    m_textLabel = new QLabel(m_container);
    m_textLabel->setWordWrap(true);
    m_textLabel->setAlignment(Qt::AlignCenter);

    const bool isMuted = (getConfig().audio.musicVolume == 0 && getConfig().audio.soundVolume == 0);
    if (isMuted) {
        m_textLabel->setText(tr("Experience MMapper with immersive music and sound effects!"));
    } else {
        m_textLabel->setText(
            tr("Browser limitations require a user interaction to enable audio playback."));
    }
    layout->addWidget(m_textLabel);

    m_button = new QPushButton(isMuted ? tr("Enable Audio") : tr("Unlock Audio"), m_container);
    m_button->setMinimumHeight(40);
    m_button->setStyleSheet("QPushButton {"
                            "  background-color: #4a90e2;"
                            "  color: white;"
                            "  border-radius: 5px;"
                            "  font-weight: bold;"
                            "  font-size: 14px;"
                            "}"
                            "QPushButton:hover {"
                            "  background-color: #357abd;"
                            "}");
    layout->addWidget(m_button);

    connect(m_button, &QPushButton::clicked, this, [this, isMuted]() {
        if (isMuted) {
            setConfig().audio.musicVolume = 50;
            setConfig().audio.soundVolume = 50;
        }

        m_audioManager.unblockAudio();
        m_audioManager.slot_updateVolumes();

        this->close();
    });

    m_container->setFixedSize(300, 250);
    updatePosition();

    connect(&m_audioManager, &AudioManager::sig_audioUnblocked, this, &AudioHint::close);
}

AudioHint::~AudioHint() = default;

void AudioHint::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.fillRect(rect(), QColor(0, 0, 0, 128));
}

void AudioHint::resizeEvent(QResizeEvent *)
{
    updatePosition();
}

void AudioHint::updatePosition()
{
    if (parentWidget()) {
        setGeometry(parentWidget()->rect());
        m_container->move((width() - m_container->width()) / 2,
                          (height() - m_container->height()) / 2);
    }
}
