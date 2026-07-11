#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "../global/macros.h"

#include <QHash>
#include <QImage>
#include <QMutex>
#include <QQuickImageProvider>
#include <QString>

// Resolves the "image://groupicons/<std|inv>/<position|affect>/<name>" URLs
// produced by GroupModel's StateIconsRole (see the scheme documented atop
// GroupModel.h) back to the same icon files GroupDelegate::paint() draws via
// getIconFilename(), inverting the pixels for the "inv" variant exactly like
// GroupImageCache does in groupwidget.cpp.
//
// requestImage() runs on a QML (render/loader) thread, so the per-id QImage
// cache is guarded by a mutex.
class NODISCARD_QOBJECT GroupIconProvider final : public QQuickImageProvider
{
private:
    QMutex m_mutex;
    QHash<QString, QImage> m_cache;

public:
    explicit GroupIconProvider();
    ~GroupIconProvider() final;

public:
    NODISCARD QImage requestImage(const QString &id,
                                  QSize *size,
                                  const QSize &requestedSize) override;
};
