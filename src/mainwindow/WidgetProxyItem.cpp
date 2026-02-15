// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2024 The MMapper Authors

#include "WidgetProxyItem.h"

#include <QQuickWindow>

WidgetProxyItem::WidgetProxyItem(QQuickItem *parent)
    : QQuickItem(parent)
{
    setFlag(ItemHasContents, false);
}

void WidgetProxyItem::setWidget(QWidget *widget)
{
    if (m_widget == widget)
        return;
    m_widget = widget;
    if (m_widget) {
        updateWidgetGeometry();
        updateWidgetVisibility();
    }
    emit widgetChanged();
}

void WidgetProxyItem::itemChange(ItemChange change, const ItemChangeData &data)
{
    QQuickItem::itemChange(change, data);
    if (change == ItemVisibleHasChanged || change == ItemSceneChange) {
        updateWidgetVisibility();
    }
}

void WidgetProxyItem::geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    QQuickItem::geometryChange(newGeometry, oldGeometry);
    updateWidgetGeometry();
}

void WidgetProxyItem::updateWidgetGeometry()
{
    if (!m_widget || !window())
        return;

    // Map item coordinates to window coordinates
    QPointF scenePos = mapToScene(QPointF(0, 0));
    m_widget->setGeometry(static_cast<int>(scenePos.x()),
                          static_cast<int>(scenePos.y()),
                          static_cast<int>(width()),
                          static_cast<int>(height()));
}

void WidgetProxyItem::updateWidgetVisibility()
{
    if (!m_widget)
        return;
    m_widget->setVisible(isVisible() && window() != nullptr);
}
