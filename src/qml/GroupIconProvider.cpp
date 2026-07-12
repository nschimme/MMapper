// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "GroupIconProvider.h"

#include "../display/Filenames.h"
#include "../group/mmapper2character.h"

#include <optional>

#include <QDebug>
#include <QHash>
#include <QMutexLocker>
#include <QStringList>

namespace { // anonymous

NODISCARD std::optional<CharacterPositionEnum> lookupPosition(const QString &name)
{
    static const QHash<QString, CharacterPositionEnum> map = [] {
        QHash<QString, CharacterPositionEnum> result;
#define X_INSERT(UPPER_CASE, lower_case, CamelCase, friendly) \
    result.insert(QStringLiteral(#lower_case), CharacterPositionEnum::UPPER_CASE);
        XFOREACH_CHARACTER_POSITION(X_INSERT)
#undef X_INSERT
        return result;
    }();
    const auto it = map.find(name);
    if (it == map.end()) {
        return std::nullopt;
    }
    return *it;
}

NODISCARD std::optional<CharacterAffectEnum> lookupAffect(const QString &name)
{
    static const QHash<QString, CharacterAffectEnum> map = [] {
        QHash<QString, CharacterAffectEnum> result;
#define X_INSERT(UPPER_CASE, lower_case, CamelCase, friendly) \
    result.insert(QStringLiteral(#lower_case), CharacterAffectEnum::UPPER_CASE);
        XFOREACH_CHARACTER_AFFECT(X_INSERT)
#undef X_INSERT
        return result;
    }();
    const auto it = map.find(name);
    if (it == map.end()) {
        return std::nullopt;
    }
    return *it;
}

} // namespace

GroupIconProvider::GroupIconProvider()
    : QQuickImageProvider(QQuickImageProvider::Image)
{}

GroupIconProvider::~GroupIconProvider() = default;

QImage GroupIconProvider::requestImage(const QString &id,
                                       QSize *const size,
                                       const QSize & /*requestedSize*/)
{
    // id is everything after "image://groupicons/", i.e. "<std|inv>/<kind>/<name>".
    const QStringList parts = id.split(QChar('/'));

    {
        QMutexLocker<QMutex> locker(&m_mutex);
        const auto it = m_cache.find(id);
        if (it != m_cache.end()) {
            if (size != nullptr) {
                *size = it->size();
            }
            return *it;
        }
    }

    if (parts.size() != 3) {
        qWarning() << "GroupIconProvider: unparseable id" << id;
        if (size != nullptr) {
            *size = QSize();
        }
        return QImage();
    }

    const QString &variant = parts.at(0);
    const QString &kind = parts.at(1);
    const QString &name = parts.at(2);

    const bool invert = (variant == QStringLiteral("inv"));
    if (!invert && variant != QStringLiteral("std")) {
        qWarning() << "GroupIconProvider: unknown variant in id" << id;
        if (size != nullptr) {
            *size = QSize();
        }
        return QImage();
    }

    QString filename;
    if (kind == QStringLiteral("position")) {
        const auto position = lookupPosition(name);
        if (!position) {
            qWarning() << "GroupIconProvider: unknown position in id" << id;
            if (size != nullptr) {
                *size = QSize();
            }
            return QImage();
        }
        filename = getIconFilename(*position);
    } else if (kind == QStringLiteral("affect")) {
        const auto affect = lookupAffect(name);
        if (!affect) {
            qWarning() << "GroupIconProvider: unknown affect in id" << id;
            if (size != nullptr) {
                *size = QSize();
            }
            return QImage();
        }
        filename = getIconFilename(*affect);
    } else {
        qWarning() << "GroupIconProvider: unknown kind in id" << id;
        if (size != nullptr) {
            *size = QSize();
        }
        return QImage();
    }

    QImage image;
    if (!image.load(filename)) {
        qWarning() << "GroupIconProvider: failed to load" << filename << "for id" << id;
        if (size != nullptr) {
            *size = QSize();
        }
        return QImage();
    }

    // Mirrors GroupImageCache::getImage() in groupwidget.cpp.
    if (invert) {
        image.invertPixels();
    }

    if (size != nullptr) {
        *size = image.size();
    }

    QMutexLocker<QMutex> locker(&m_mutex);
    return m_cache.insert(id, image).value();
}
