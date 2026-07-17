// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "DescriptionAdapter.h"

#include "../configuration/configuration.h"
#include "../global/Charset.h"
#include "../global/ConfigConsts-Computed.h"
#include "../map/mmapper2room.h"
#include "../preferences/AnsiColorTables.h"
#include "MediaLibrary.h"

#include <memory>

#include <QDebug>
#include <QFont>
#include <QMutexLocker>
#include <QRegularExpression>

DescriptionAdapter::DescriptionAdapter(MediaLibrary &library, QObject *const parent)
    : QObject(parent)
    , m_library(library)
    , m_store(std::make_shared<DescriptionImageStore>())
{
    resolveConfig();

    if (m_library.numImages() == 0) {
        qWarning().noquote() << "[description] MediaLibrary has no image files indexed; room/area"
                                "backgrounds will never appear. Install background images under"
                             << (getConfig().canvas.resourcesDirectory + "/areas") << "or"
                             << (getConfig().canvas.resourcesDirectory + "/rooms")
                             << "(Preferences > General > Resources Directory controls this path).";
    }

    connect(&m_library, &MediaLibrary::sig_mediaChanged, this, [this]() {
        m_imageCache.clear();
        resolveImage();
    });
}

void DescriptionAdapter::resolveConfig()
{
    auto toColor = [](const QString &str) -> QColor {
        AnsiColorTables::ParsedColor color = AnsiColorTables::colorFromString(str);
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
            qWarning() << "[description] Failed to load image:" << imagePath;
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
        if (m_store->base.isNull()) {
            qInfo() << "[description] resolveImage: cleared (no image); rev" << m_store->rev;
        } else {
            qInfo() << "[description] resolveImage: rev" << m_store->rev << "size"
                    << m_store->base.size();
        }
    }

    emit sig_changed();
}

void DescriptionAdapter::setImageForTesting(const QImage &image)
{
    {
        QMutexLocker<QMutex> locker(&m_store->mutex);
        m_store->base = image;
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
        m_lastLookupSummary.clear();
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
    const QString roomKey = QString::number(id.asUint32());
    if (id != INVALID_SERVER_ROOMID) {
        newFileName = m_library.findImage("rooms", roomKey);
    }

    const RoomArea &area = r.getArea();
    static const QRegularExpression regex("^the\\s+");
    QString areaName = area.toQString().toLower().remove(regex).replace(' ', '-');
    mmqt::toAsciiInPlace(areaName);

    if (newFileName.isEmpty()) {
        newFileName = m_library.findImage("areas", areaName);
    }

    m_roomName = r.getName().toQString();
    m_roomDesc = r.getDescription().toQString().simplified();

    const QString roomsTried = "rooms/" + roomKey;
    const QString areasTried = "areas/" + areaName;
    m_lastLookupSummary = newFileName.isEmpty()
                              ? QStringLiteral("no image: %1, %2").arg(roomsTried, areasTried)
                              : QString();

    if (newFileName != m_fileName) {
        m_fileName = newFileName;
        if (newFileName.isEmpty()) {
            qInfo() << "[description] updateRoom: no image found for room; serverId" << roomKey
                    << "tried" << roomsTried << "and" << areasTried;
        } else {
            qInfo() << "[description] updateRoom: serverId" << roomKey << "resolved to"
                    << newFileName;
        }
        resolveImage(); // emits sig_changed()
    } else {
        emit sig_changed();
    }
}
