#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "../global/macros.h"

#include <QDockWidget>

class QQuickWidget;
class QQuickImageProvider;

class NODISCARD_QOBJECT QmlDockWidget final : public QDockWidget
{
    Q_OBJECT

private:
    QQuickWidget *m_quick = nullptr;
    bool m_statusConnected = false;

public:
    explicit QmlDockWidget(const QString &title, const QString &objectName, QWidget *parent);

    // Must be called before setQmlSource(); the root context must have all
    // properties before the root object is instantiated.
    void setContextProperty(const QString &name, QObject *object);

    // Must be called before setQmlSource() so the provider is registered
    // with the engine before the QML loads and requests images from it.
    // The QQmlEngine takes ownership of the provider.
    void addImageProvider(const QString &id, QQuickImageProvider *provider);

    void setQmlSource(const QUrl &url);

    NODISCARD QQuickWidget *quickWidget() const { return m_quick; }

private:
    // Copies the root QML item's implicitWidth/implicitHeight (if positive)
    // onto the QQuickWidget's minimumSize; see setQmlSource()'s Ready
    // handler.
    void syncMinimumSize();
};
