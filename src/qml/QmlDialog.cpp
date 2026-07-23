// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "QmlDialog.h"

#include <algorithm>

#include <QLabel>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQmlError>
#include <QQuickImageProvider>
#include <QQuickItem>
#include <QQuickWidget>
#include <QQuickWindow>
#include <QScreen>
#include <QVBoxLayout>

namespace { // anonymous
constexpr int MIN_DIALOG_WIDTH = 320;
constexpr int MIN_DIALOG_HEIGHT = 200;
} // namespace

QmlDialog::QmlDialog(const QString &title, const QString &objectName, QWidget *const parent)
    : QDialog(parent)
{
    setWindowTitle(title);
    setObjectName(objectName);

    m_quick = new QQuickWidget(this);
    m_quick->setResizeMode(QQuickWidget::SizeRootObjectToView);
    m_quick->setFocusPolicy(Qt::StrongFocus);
    // QQuickWidget's default clear color is white, which shows through until
    // the QML root paints over it (and can bleed through translucent edges).
    // Match the host palette so the dialog never flashes white on dark themes.
    m_quick->setClearColor(palette().color(QPalette::Window));

    auto *const layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_quick);

    // QDialog::accept()/reject() are slots, so QML can call dialog.accept()/
    // dialog.reject() directly once this is exposed as the "dialog" context
    // property. Must happen before setQmlSource() for the same reason as any
    // other context property (see setContextProperty()'s doc comment).
    setContextProperty("dialog", this);
}

void QmlDialog::setContextProperty(const QString &name, QObject *const object)
{
    if (m_quick == nullptr) {
        return;
    }
    m_quick->rootContext()->setContextProperty(name, object);
}

void QmlDialog::addImageProvider(const QString &id, QQuickImageProvider *const provider)
{
    if (m_quick == nullptr) {
        qWarning() << "QmlDialog::addImageProvider() called with no QQuickWidget";
        delete provider;
        return;
    }
    // engine()->addImageProvider() takes ownership of provider.
    m_quick->engine()->addImageProvider(id, provider);
}

void QmlDialog::setQmlSource(const QUrl &url)
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
            if (status == QQuickWidget::Ready) {
                if (QQuickItem *const root = m_quick->rootObject()) {
                    int width = std::max(MIN_DIALOG_WIDTH, static_cast<int>(root->implicitWidth()));
                    int height = std::max(MIN_DIALOG_HEIGHT,
                                          static_cast<int>(root->implicitHeight()));
                    // Clamp to the available screen geometry so a dialog never
                    // exceeds the screen (e.g. a phone/small-monitor session
                    // where implicit content size is larger than the display).
                    // Applied after the MIN clamp so the screen always wins.
                    if (const QScreen *const scr = screen()) {
                        const QRect avail = scr->availableGeometry();
                        width = std::min(width, avail.width());
                        height = std::min(height, avail.height());
                    }
                    resize(width, height);
                }
                return;
            }
            if (status != QQuickWidget::Error) {
                return;
            }
            for (const QQmlError &error : m_quick->errors()) {
                qWarning() << "QML load error:" << error.toString();
            }
            // Replace the broken QML view with a plain fallback so a bad
            // QML file never takes the whole dialog down.
            auto *const fallback = new QLabel(tr("QML load failed"), this);
            fallback->setAlignment(Qt::AlignCenter);
            if (QLayout *const dialogLayout = layout()) {
                dialogLayout->removeWidget(m_quick);
                dialogLayout->addWidget(fallback);
            }
            m_quick->deleteLater();
            m_quick = nullptr;
        });

        connect(m_quick,
                &QQuickWidget::sceneGraphError,
                this,
                [](QQuickWindow::SceneGraphError error, const QString &message) {
                    qWarning() << "QML scene graph error:" << static_cast<int>(error) << message;
                });
    }

    m_quick->setSource(url);
}
