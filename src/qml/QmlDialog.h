#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "../global/macros.h"

#include <QDialog>

class QQuickWidget;
class QQuickImageProvider;

// A QDialog that hosts a single QQuickWidget filling its client area, mirroring
// QmlDockWidget's role for docks. The dialog itself is exposed to the QML root
// as the context property "dialog" so QML can call its accept()/reject() slots
// directly (e.g. a Close button binds to dialog.reject()).
class NODISCARD_QOBJECT QmlDialog : public QDialog
{
    Q_OBJECT

private:
    QQuickWidget *m_quick = nullptr;
    bool m_statusConnected = false;

public:
    explicit QmlDialog(const QString &title, const QString &objectName, QWidget *parent);

    // Must be called before setQmlSource(); the root context must have all
    // properties before the root object is instantiated.
    void setContextProperty(const QString &name, QObject *object);

    // Must be called before setQmlSource() so the provider is registered
    // with the engine before the QML loads and requests images from it.
    // The QQmlEngine takes ownership of the provider.
    void addImageProvider(const QString &id, QQuickImageProvider *provider);

    void setQmlSource(const QUrl &url);

    NODISCARD QQuickWidget *quickWidget() const { return m_quick; }
};
