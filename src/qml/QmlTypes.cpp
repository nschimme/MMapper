// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "QmlTypes.h"

#include "../display/MapCanvasQuickItem.h"

#include <QQmlEngine>

void registerMmQmlTypes()
{
    qmlRegisterType<MapCanvasQuickItem>("MMapper", 1, 0, "MapCanvasItem");
}
