// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "DescriptionImageProvider.h"

#include "../media/StackBlur.h"

#include <algorithm>
#include <utility>

#include <QDebug>
#include <QMutexLocker>

// Mirrors DescriptionWidget::updateBackground()'s constants (DescriptionWidget.cpp).
static constexpr int BASE_BLUR_RADIUS = 16;
static constexpr int DOWNSCALE_FACTOR = 10;

DescriptionImageProvider::DescriptionImageProvider(std::shared_ptr<DescriptionImageStore> store)
    : QQuickImageProvider(QQuickImageProvider::Image)
    , m_store(std::move(store))
{}

DescriptionImageProvider::~DescriptionImageProvider() = default;

QImage DescriptionImageProvider::requestImage(const QString &id,
                                              QSize *const size,
                                              const QSize & /*requestedSize*/)
{
    // id is everything after "image://description/", i.e. "<sharp|blur>/<rev>".
    const qsizetype slash = id.indexOf(QChar('/'));
    const QString kind = slash < 0 ? id : id.left(slash);

    QImage base;
    {
        QMutexLocker<QMutex> locker(&m_store->mutex);
        base = m_store->base;
    }

    if (base.isNull()) {
        qWarning() << "DescriptionImageProvider: no base image available for id" << id;
        if (size != nullptr) {
            *size = QSize();
        }
        return QImage();
    }

    if (kind == QStringLiteral("sharp")) {
        if (size != nullptr) {
            *size = base.size();
        }
        return base;
    }

    if (kind == QStringLiteral("blur")) {
        const QSize downscaledSize(std::max(1, base.width() / DOWNSCALE_FACTOR),
                                   std::max(1, base.height() / DOWNSCALE_FACTOR));
        QImage blurSource = base.scaled(downscaledSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
                                .convertToFormat(QImage::Format_ARGB32_Premultiplied);

        if (blurSource.isNull() || blurSource.width() == 0 || blurSource.height() == 0) {
            qWarning() << "DescriptionImageProvider: blur source invalid for id" << id;
            if (size != nullptr) {
                *size = QSize();
            }
            return QImage();
        }

        const int blurRadius = std::max(0,
                                        std::min(std::min(BASE_BLUR_RADIUS / DOWNSCALE_FACTOR,
                                                          (blurSource.width() - 1) / 2),
                                                 (blurSource.height() - 1) / 2));
        mm::stackBlur(blurSource, blurRadius);

        if (size != nullptr) {
            *size = blurSource.size();
        }
        return blurSource;
    }

    qWarning() << "DescriptionImageProvider: unknown kind in id" << id;
    if (size != nullptr) {
        *size = QSize();
    }
    return QImage();
}
