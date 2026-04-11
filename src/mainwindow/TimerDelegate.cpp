// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2024 The MMapper Authors

#include "TimerDelegate.h"

#include "TimerModel.h"

#include <QColor>
#include <QPainter>

TimerDelegate::TimerDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
{}

void TimerDelegate::paint(QPainter *painter,
                          const QStyleOptionViewItem &option,
                          const QModelIndex &index) const
{
    QVariant progressVar = index.data(TimerModel::ProgressRole);
    if (progressVar.isValid() && index.column() == TimerModel::ColName) {
        double progress = progressVar.toDouble();

        painter->save();

        // Draw background bar
        QRect rect = option.rect;
        int barWidth = static_cast<int>(static_cast<double>(rect.width()) * progress);

        QColor color;
        if (progress > 0.5) {
            // Green to Yellow
            double factor = (progress - 0.5) * 2.0;
            color = QColor::fromRgbF(1.0 - factor, 1.0, 0.0, 0.3);
        } else {
            // Yellow to Red
            double factor = progress * 2.0;
            color = QColor::fromRgbF(1.0, factor, 0.0, 0.3);
        }

        painter->fillRect(rect.x(), rect.y(), barWidth, rect.height(), color);

        painter->restore();
    }

    QStyledItemDelegate::paint(painter, option, index);
}
