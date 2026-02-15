#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2024 The MMapper Authors

#include <QPointer>
#include <QQuickItem>
#include <QWidget>

class WidgetProxyItem : public QQuickItem
{
    Q_OBJECT
    Q_PROPERTY(QWidget *widget READ widget WRITE setWidget NOTIFY widgetChanged)

public:
    explicit WidgetProxyItem(QQuickItem *parent = nullptr);

    QWidget *widget() const { return m_widget; }
    void setWidget(QWidget *widget);

signals:
    void widgetChanged();

protected:
    void itemChange(ItemChange change, const ItemChangeData &data) override;
    void geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) override;

private:
    void updateWidgetGeometry();
    void updateWidgetVisibility();

    QPointer<QWidget> m_widget;
};
