// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "QmlDockWidget.h"

#include <QLabel>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQmlError>
#include <QQuickImageProvider>
#include <QQuickItem>
#include <QQuickWidget>
#include <QQuickWindow>
#include <QtMath>

QmlDockWidget::QmlDockWidget(const QString &title, const QString &objectName, QWidget *parent)
    : QDockWidget(title, parent)
{
    setObjectName(objectName);

    m_quick = new QQuickWidget(this);
    m_quick->setResizeMode(QQuickWidget::SizeRootObjectToView);
    m_quick->setFocusPolicy(Qt::StrongFocus);
    // QQuickWidget's default clear color is white, which shows through until
    // the QML root paints over it (and can bleed through translucent edges).
    // Match the host palette so the dock never flashes white on dark themes.
    m_quick->setClearColor(palette().color(QPalette::Window));
    setWidget(m_quick);
}

void QmlDockWidget::setContextProperty(const QString &name, QObject *const object)
{
    if (m_quick == nullptr) {
        return;
    }
    m_quick->rootContext()->setContextProperty(name, object);
}

void QmlDockWidget::addImageProvider(const QString &id, QQuickImageProvider *const provider)
{
    if (m_quick == nullptr) {
        qWarning() << "QmlDockWidget::addImageProvider() called with no QQuickWidget";
        delete provider;
        return;
    }
    // engine()->addImageProvider() takes ownership of provider.
    m_quick->engine()->addImageProvider(id, provider);
}

void QmlDockWidget::setQmlSource(const QUrl &url)
{
    if (m_quick == nullptr) {
        return;
    }

    // connect() must happen before setSource() below: the root context must
    // have all properties before the root object is instantiated, and we
    // want to observe the very first status change. Lambdas can't be used
    // with Qt::UniqueConnection, so guard against double-connecting manually
    // in case setQmlSource() is ever called more than once on this instance.
    if (!m_statusConnected) {
        m_statusConnected = true;

        connect(m_quick, &QQuickWidget::statusChanged, this, [this](QQuickWidget::Status status) {
            if (status != QQuickWidget::Error) {
                return;
            }
            for (const QQmlError &error : m_quick->errors()) {
                qWarning() << "QML load error:" << error.toString();
            }
            // Replace the broken QML view with a plain fallback so a bad
            // QML file never takes the whole application down.
            auto *const fallback = new QLabel(tr("QML load failed"), this);
            fallback->setAlignment(Qt::AlignCenter);
            setWidget(fallback);
            m_quick->deleteLater();
            m_quick = nullptr;
        });

        connect(m_quick,
                &QQuickWidget::sceneGraphError,
                this,
                [](QQuickWindow::SceneGraphError error, const QString &message) {
                    qWarning() << "QML scene graph error:" << static_cast<int>(error) << message;
                });

        // The QML panels (see e.g. ClientPanel.qml, GroupPanel.qml) declare
        // an implicitWidth/implicitHeight on their root item to say how
        // small they can usefully get, mirroring the widget-era
        // sizeHint()/minimumSizeHint() overrides (e.g.
        // GroupWidget::sizeHint(), DisplayWidget::minimumSizeHint()) that
        // those panels replaced. QQuickWidget never reads implicitWidth/
        // implicitHeight on its own, so without this the dock would let the
        // user squash the panel down to nothing. Sync it once the root
        // object is ready, then keep it live in case implicitWidth/Height
        // ever changes at runtime (e.g. font/config changes).
        connect(m_quick, &QQuickWidget::statusChanged, this, [this](QQuickWidget::Status status) {
            if (status != QQuickWidget::Ready) {
                return;
            }
            QQuickItem *const root = m_quick->rootObject();
            if (root == nullptr) {
                return;
            }
            syncMinimumSize();
            connect(root, &QQuickItem::implicitWidthChanged, this, &QmlDockWidget::syncMinimumSize);
            connect(root, &QQuickItem::implicitHeightChanged, this, &QmlDockWidget::syncMinimumSize);
        });
    }

    m_quick->setSource(url);
}

void QmlDockWidget::syncMinimumSize()
{
    if (m_quick == nullptr) {
        return;
    }
    QQuickItem *const root = m_quick->rootObject();
    if (root == nullptr) {
        return;
    }
    const qreal w = root->implicitWidth();
    const qreal h = root->implicitHeight();
    if (w > 0 && h > 0) {
        m_quick->setMinimumSize(QSize(qCeil(w), qCeil(h)));
    }
}
