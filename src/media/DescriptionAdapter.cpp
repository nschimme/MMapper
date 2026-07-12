// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "DescriptionAdapter.h"

#include "../configuration/configuration.h"
#include "../global/Charset.h"
#include "../global/ConfigConsts-Computed.h"
#include "../map/mmapper2room.h"
#include "../preferences/ansicombo.h"
#include "MediaLibrary.h"

#include <memory>

#include <QFont>
#include <QMutexLocker>
#include <QRegularExpression>

DescriptionAdapter::DescriptionAdapter(MediaLibrary &library, QObject *const parent)
    : QObject(parent)
    , m_library(library)
    , m_store(std::make_shared<DescriptionImageStore>())
{
    resolveConfig();

    connect(&m_library, &MediaLibrary::sig_mediaChanged, this, [this]() {
        m_imageCache.clear();
        resolveImage();
    });
}

void DescriptionAdapter::resolveConfig()
{
    auto toColor = [](const QString &str) -> QColor {
        AnsiCombo::AnsiColor color = AnsiCombo::colorFromString(str);
        if (color.fg.hasColor()) {
            return color.getFgColor();
        }
        return getConfig().integratedClient.foregroundColor;
    };

    m_nameColor = toColor(getConfig().parser.roomNameColor);
    m_descColor = toColor(getConfig().parser.roomDescColor);
    m_bgColor = getConfig().integratedClient.backgroundColor;
    m_fgColor = getConfig().integratedClient.foregroundColor;

    QFont font;
    font.fromString(getConfig().integratedClient.font);
    m_fontFamily = font.family();
    m_fontPointSize = font.pointSize();
}

void DescriptionAdapter::reloadConfig()
{
    resolveConfig();
    emit sig_changed();
}

QImage *DescriptionAdapter::loadAndCacheImage(const QString &imagePath)
{
    if (imagePath.isEmpty()) {
        return nullptr;
    }

    if (QImage *cachedImage = m_imageCache.object(imagePath)) {
        return cachedImage;
    }

    if constexpr (CURRENT_PLATFORM == PlatformEnum::Wasm) {
        m_library.fetchAsync(imagePath, [this, imagePath](const QByteArray &data) {
            QImage img;
            if (img.loadFromData(data)) {
                if (!img.isNull()) {
                    m_imageCache.insert(imagePath, new QImage(img));
                }
                if (imagePath == m_fileName) {
                    resolveImage();
                }
            }
        });
        return nullptr;
    } else {
        std::unique_ptr<QImage> temp = std::make_unique<QImage>();
        if (!temp->load(imagePath) || temp->isNull()) {
            qWarning() << "Failed to load image:" << imagePath;
            return nullptr;
        }
        m_imageCache.insert(imagePath, temp.release());
        return m_imageCache.object(imagePath);
    }
}

void DescriptionAdapter::resolveImage()
{
    QImage *const pImage = loadAndCacheImage(m_fileName);

    {
        QMutexLocker<QMutex> locker(&m_store->mutex);
        m_store->base = (pImage && !pImage->isNull()) ? *pImage : QImage();
        ++m_store->rev;
    }

    emit sig_changed();
}

QUrl DescriptionAdapter::getImageUrl() const
{
    QMutexLocker<QMutex> locker(&m_store->mutex);
    if (m_store->base.isNull()) {
        return QUrl();
    }
    return QUrl(QStringLiteral("image://description/sharp/%1").arg(m_store->rev));
}

QUrl DescriptionAdapter::getBlurUrl() const
{
    QMutexLocker<QMutex> locker(&m_store->mutex);
    if (m_store->base.isNull()) {
        return QUrl();
    }
    return QUrl(QStringLiteral("image://description/blur/%1").arg(m_store->rev));
}

void DescriptionAdapter::updateRoom(const RoomHandle &r)
{
    if (!r) {
        m_roomName.clear();
        m_roomDesc.clear();
        if (!m_fileName.isEmpty()) {
            m_fileName.clear();
            resolveImage(); // emits sig_changed()
        } else {
            emit sig_changed();
        }
        return;
    }

    QString newFileName;
    const ServerRoomId id = r.getServerId();
    if (id != INVALID_SERVER_ROOMID) {
        newFileName = m_library.findImage("rooms", QString::number(id.asUint32()));
    }

    if (newFileName.isEmpty()) {
        const RoomArea &area = r.getArea();
        static const QRegularExpression regex("^the\\s+");
        QString areaName = area.toQString().toLower().remove(regex).replace(' ', '-');
        mmqt::toAsciiInPlace(areaName);
        newFileName = m_library.findImage("areas", areaName);
    }

    m_roomName = r.getName().toQString();
    m_roomDesc = r.getDescription().toQString().simplified();

    if (newFileName != m_fileName) {
        m_fileName = newFileName;
        resolveImage(); // emits sig_changed()
    } else {
        emit sig_changed();
    }
}
