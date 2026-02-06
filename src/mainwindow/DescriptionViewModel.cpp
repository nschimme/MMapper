// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#include "DescriptionViewModel.h"
#include "../configuration/configuration.h"
#include "../global/Charset.h"
#include "../preferences/ansicombo.h"
#include <QDirIterator>
#include <QFileInfo>
#include <QImageReader>
#include <QRegularExpression>
#include <QPainter>
#include <QDebug>

static constexpr int BASE_BLUR_RADIUS = 16;
static constexpr int DOWNSCALE_FACTOR = 10;

DescriptionViewModel::DescriptionViewModel(QObject *p) : QObject(p) {
    scanDirectories();
}

QColor DescriptionViewModel::backgroundColor() const { return getConfig().integratedClient.backgroundColor; }
QColor DescriptionViewModel::roomNameColor() const { return toColor(getConfig().parser.roomNameColor); }
QColor DescriptionViewModel::roomDescColor() const { return toColor(getConfig().parser.roomDescColor); }

QColor DescriptionViewModel::toColor(const QString &str) const {
    AnsiCombo::AnsiColor color = AnsiCombo::colorFromString(str);
    return color.fg.hasColor() ? color.getFgColor() : getConfig().integratedClient.foregroundColor;
}

void DescriptionViewModel::scanDirectories() {
    m_availableFiles.clear();
    QByteArrayList supportedFormatsList = QImageReader::supportedImageFormats();
    std::set<QString> supportedFormats;
    for (const QByteArray &format : supportedFormatsList) supportedFormats.insert(QString::fromUtf8(format).toLower());

    auto scanPath = [&](const QString &path) {
        QDirIterator it(path, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            QFileInfo fileInfo(it.next());
            QString suffix = fileInfo.suffix();
            if (supportedFormats.find(suffix.toLower()) != supportedFormats.end()) {
                QString filePath = fileInfo.filePath();
                auto dotIndex = filePath.lastIndexOf('.');
                if (dotIndex == -1) continue;
                const bool isQrc = filePath.startsWith(":/");
                QString baseName;
                if (isQrc) baseName = filePath.left(dotIndex);
                else {
                    QString resourcesPath = getConfig().canvas.resourcesDirectory;
                    if (!filePath.startsWith(resourcesPath)) continue;
                    baseName = filePath.mid(resourcesPath.length()).left(dotIndex - resourcesPath.length());
                }
                if (!baseName.isEmpty()) m_availableFiles.insert({baseName, suffix});
            }
        }
    };
    const auto &resourcesDir = getConfig().canvas.resourcesDirectory;
    scanPath(resourcesDir + "/rooms");
    scanPath(resourcesDir + "/areas");
    scanPath(":/rooms");
    scanPath(":/areas");
}

void DescriptionViewModel::setWidgetSize(const QSize &size) {
    if (m_widgetSize != size) {
        m_widgetSize = size;
        updateBlurredImage();
    }
}

void DescriptionViewModel::updateRoom(const RoomHandle &r) {
    m_room = r;
    QString oldPath = m_backgroundImagePath;
    if (!r) {
        m_backgroundImagePath = "";
        m_roomName = "";
        m_roomDescription = "";
    } else {
        m_roomName = r.getName().toQString();
        m_roomDescription = r.getDescription().toQString().simplified();

        auto findImage = [&](const QString &base) -> std::optional<QString> {
            auto it = m_availableFiles.find("/" + base);
            if (it != m_availableFiles.end()) return it->first + "." + it->second;
            it = m_availableFiles.find(":/" + base);
            if (it != m_availableFiles.end()) return it->first + "." + it->second;
            return std::nullopt;
        };

        QString newPath;
        const ServerRoomId id = r.getServerId();
        if (id != INVALID_SERVER_ROOMID) {
            if (auto opt = findImage(QString("rooms/%1").arg(id.asUint32()))) newPath = *opt;
        }
        if (newPath.isEmpty()) {
            const RoomArea &area = r.getArea();
            static const QRegularExpression regex("^the\\s+");
            QString base = area.toQString().toLower().remove(regex).replace(' ', '-');
            mmqt::toAsciiInPlace(base);
            if (auto opt = findImage("areas/" + base)) newPath = *opt;
        }
        m_backgroundImagePath = newPath;
    }
    emit roomNameChanged();
    emit roomDescriptionChanged();
    if (oldPath != m_backgroundImagePath) {
        emit backgroundImagePathChanged();
        updateBlurredImage();
    }
}

void DescriptionViewModel::updateBlurredImage() {
    if (m_backgroundImagePath.isEmpty() || m_widgetSize.isEmpty()) {
        m_blurredImage = QImage();
        emit blurredImageChanged();
        return;
    }

    QString fullPath = m_backgroundImagePath;
    if (!m_backgroundImagePath.startsWith(":/")) {
        fullPath = getConfig().canvas.resourcesDirectory + m_backgroundImagePath;
    }

    QImage baseImage(fullPath);
    if (baseImage.isNull()) {
        m_blurredImage = QImage();
        emit blurredImageChanged();
        return;
    }

    QImage resultImage(m_widgetSize, QImage::Format_ARGB32_Premultiplied);
    resultImage.fill(Qt::transparent);
    QPainter painter(&resultImage);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    // Blur background
    QSize downscaledBlurSourceSize = QSize(std::max(1, m_widgetSize.width() / DOWNSCALE_FACTOR),
                                          std::max(1, m_widgetSize.height() / DOWNSCALE_FACTOR));
    QImage blurSource = baseImage.scaled(downscaledBlurSourceSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
                                 .convertToFormat(QImage::Format_ARGB32_Premultiplied);

    int blurRadius = std::max(0, std::min(std::min(BASE_BLUR_RADIUS / DOWNSCALE_FACTOR, (blurSource.width() - 1) / 2),
                                          (blurSource.height() - 1) / 2));
    if (blurRadius > 0) {
        stackBlur(blurSource, blurRadius);
    }
    QImage fullBlurredBgImage = blurSource.scaled(m_widgetSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    painter.drawImage(0, 0, fullBlurredBgImage);

    // Draw scaled image in center
    QImage scaledImage = baseImage.scaled(m_widgetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    QPoint center((m_widgetSize.width() - scaledImage.width()) / 2, (m_widgetSize.height() - scaledImage.height()) / 2);
    painter.drawImage(center, scaledImage);
    painter.end();

    m_blurredImage = resultImage;
    emit blurredImageChanged();
}

void DescriptionViewModel::stackBlur(QImage &image, int radius) {
    const int w = image.width();
    const int h = image.height();
    const int div = 2 * radius + 1;
    std::vector<QRgb> linePixels;

    // Horizontal blur
    linePixels.resize(static_cast<size_t>(w));
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) linePixels[static_cast<size_t>(x)] = image.pixel(x, y);
        for (int x = 0; x < w; ++x) {
            int rSum = 0, gSum = 0, bSum = 0, aSum = 0;
            for (int i = -radius; i <= radius; ++i) {
                size_t idx = static_cast<size_t>(std::clamp(x + i, 0, w - 1));
                QRgb pixel = linePixels[idx];
                rSum += qRed(pixel); gSum += qGreen(pixel); bSum += qBlue(pixel); aSum += qAlpha(pixel);
            }
            image.setPixel(x, y, qRgba(rSum / div, gSum / div, bSum / div, aSum / div));
        }
    }

    // Vertical blur
    linePixels.resize(static_cast<size_t>(h));
    for (int x = 0; x < w; ++x) {
        for (int y = 0; y < h; ++y) linePixels[static_cast<size_t>(y)] = image.pixel(x, y);
        for (int y = 0; y < h; ++y) {
            int rSum = 0, gSum = 0, bSum = 0, aSum = 0;
            for (int i = -radius; i <= radius; ++i) {
                size_t idy = static_cast<size_t>(std::clamp(y + i, 0, h - 1));
                QRgb pixel = linePixels[idy];
                rSum += qRed(pixel); gSum += qGreen(pixel); bSum += qBlue(pixel); aSum += qAlpha(pixel);
            }
            image.setPixel(x, y, qRgba(rSum / div, gSum / div, bSum / div, aSum / div));
        }
    }
}
