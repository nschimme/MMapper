#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "../global/macros.h"
#include "../media/DescriptionImageStore.h"

#include <memory>

#include <QQuickImageProvider>
#include <QString>

// Resolves the "image://description/sharp/<rev>" and
// "image://description/blur/<rev>" URLs produced by DescriptionAdapter's
// imageUrl/blurUrl properties back to the sharp source image and a blurred
// variant of it (mirroring the blur pass DescriptionWidget::updateBackground()
// used to run inline via mm::stackBlur(), see StackBlur.h). <rev> is unused
// by requestImage() itself -- it only exists so the URL changes whenever
// DescriptionAdapter::resolveImage() bumps the store's revision, forcing
// QML's image cache to reload instead of reusing a stale pixmap.
//
// The adapter and this provider have independent lifetimes (the adapter is
// owned by MainWindow, this provider by the QQmlEngine), so they share
// image data through a std::shared_ptr<DescriptionImageStore> rather than a
// raw reference; see DescriptionImageStore.h.
class NODISCARD_QOBJECT DescriptionImageProvider final : public QQuickImageProvider
{
private:
    std::shared_ptr<DescriptionImageStore> m_store;

public:
    explicit DescriptionImageProvider(std::shared_ptr<DescriptionImageStore> store);
    ~DescriptionImageProvider() final;

public:
    NODISCARD QImage requestImage(const QString &id,
                                  QSize *size,
                                  const QSize &requestedSize) override;
};
