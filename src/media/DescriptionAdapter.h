#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "../map/RoomHandle.h"
#include "../global/macros.h"
#include "DescriptionImageStore.h"

#include <memory>

#include <QCache>
#include <QColor>
#include <QImage>
#include <QObject>
#include <QString>
#include <QUrl>

class MediaLibrary;

// QML-facing replacement for DescriptionWidget. Ports its image resolution
// (MediaLibrary::findImage("rooms", ...) falling back to
// findImage("areas", ...)) and color/font resolution, but does not do any
// compositing itself: the sharp and blurred images are exposed as
// image://description/... URLs (see DescriptionImageProvider in mm_qml,
// which reads the DescriptionImageStore this class shares with it via
// getStore()) and blurring happens lazily in the provider on request.
//
// DescriptionImageProvider is not referenced here because it derives from
// QQuickImageProvider, which needs Qt6::Quick; this class lives in the
// always-compiled mmapper_SRCS, so provider construction is left to
// MainWindow (see getStore()).
class NODISCARD_QOBJECT DescriptionAdapter final : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString roomName READ getRoomName NOTIFY sig_changed)
    Q_PROPERTY(QString roomDesc READ getRoomDesc NOTIFY sig_changed)
    Q_PROPERTY(QColor nameColor READ getNameColor NOTIFY sig_changed)
    Q_PROPERTY(QColor descColor READ getDescColor NOTIFY sig_changed)
    Q_PROPERTY(QColor bgColor READ getBgColor NOTIFY sig_changed)
    Q_PROPERTY(QColor fgColor READ getFgColor NOTIFY sig_changed)
    Q_PROPERTY(QString fontFamily READ getFontFamily NOTIFY sig_changed)
    Q_PROPERTY(int fontPointSize READ getFontPointSize NOTIFY sig_changed)
    Q_PROPERTY(QUrl imageUrl READ getImageUrl NOTIFY sig_changed)
    Q_PROPERTY(QUrl blurUrl READ getBlurUrl NOTIFY sig_changed)

private:
    MediaLibrary &m_library;

    std::shared_ptr<DescriptionImageStore> m_store;
    QCache<QString, QImage> m_imageCache;
    QString m_fileName;

    QString m_roomName;
    QString m_roomDesc;

    QColor m_nameColor;
    QColor m_descColor;
    QColor m_bgColor;
    QColor m_fgColor;
    QString m_fontFamily;
    int m_fontPointSize = -1;

public:
    explicit DescriptionAdapter(MediaLibrary &library, QObject *parent = nullptr);
    ~DescriptionAdapter() final = default;

public:
    // Shared with DescriptionImageProvider; see the class comment above.
    NODISCARD std::shared_ptr<DescriptionImageStore> getStore() const { return m_store; }

    void updateRoom(const RoomHandle &r);

    // Test seam: writes `image` directly into the shared store (bumping its
    // revision) and emits sig_changed(), bypassing MediaLibrary/file
    // resolution entirely. Production code always goes through
    // updateRoom()/resolveImage(); tests use this to exercise the QML image
    // pipeline (DescriptionImageProvider, blur/sharp Image elements) without
    // needing real image files on disk.
    void setImageForTesting(const QImage &image);

public:
    NODISCARD QString getRoomName() const { return m_roomName; }
    NODISCARD QString getRoomDesc() const { return m_roomDesc; }
    NODISCARD QColor getNameColor() const { return m_nameColor; }
    NODISCARD QColor getDescColor() const { return m_descColor; }
    NODISCARD QColor getBgColor() const { return m_bgColor; }
    NODISCARD QColor getFgColor() const { return m_fgColor; }
    NODISCARD QString getFontFamily() const { return m_fontFamily; }
    NODISCARD int getFontPointSize() const { return m_fontPointSize; }
    NODISCARD QUrl getImageUrl() const;
    NODISCARD QUrl getBlurUrl() const;

public slots:
    // Re-resolves colors/font from the live Configuration and emits
    // sig_changed(). Must be called whenever code outside this class may
    // have written to getConfig().parser/integratedClient (e.g. the
    // preferences dialog); see MainWindow::slot_onPreferences().
    void reloadConfig();

signals:
    void sig_changed();

private:
    void resolveConfig();
    // Loads (and QCache-caches) the image at `imagePath`, mirroring
    // DescriptionWidget::updateBackground()'s loadAndCacheImage lambda,
    // including the Wasm MediaLibrary::fetchAsync path.
    NODISCARD QImage *loadAndCacheImage(const QString &imagePath);
    // Re-resolves m_fileName into the shared store, bumps its revision, and
    // emits sig_changed().
    void resolveImage();
};
