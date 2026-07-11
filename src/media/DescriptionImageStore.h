#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include <QImage>
#include <QMutex>

// Shared between DescriptionAdapter (owned by MainWindow) and
// DescriptionImageProvider (owned by the QQmlEngine, so it can outlive or be
// destroyed independently of the adapter/MainWindow). A std::shared_ptr to
// this struct is held by both, sidestepping the lifetime mismatch. All
// access must go through `mutex`: requestImage() runs on a QML
// loader/render thread while updateRoom() runs on the GUI thread.
struct DescriptionImageStore
{
    QMutex mutex;
    QImage base; // sharp source image; null when no image is resolved
    int rev = 0; // bumped every time `base` changes; embedded in the
                 // image://description/... URLs so QML is forced to reload
                 // rather than reuse a cached pixmap for a stale id
};
