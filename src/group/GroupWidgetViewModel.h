#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#include "../global/macros.h"
#include "mmapper2character.h"
#include <QObject>
#include <memory>
#include <glm/vec2.hpp>

class Mmapper2Group;
class MapData;

class NODISCARD_QOBJECT GroupWidgetViewModel final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool mapLoaded READ mapLoaded WRITE setMapLoaded NOTIFY mapLoadedChanged)

public:
    explicit GroupWidgetViewModel(Mmapper2Group &group, QObject *parent = nullptr);

    NODISCARD bool mapLoaded() const { return m_mapLoaded; }
    void setMapLoaded(bool loaded);

    void centerOnCharacter(const SharedGroupChar &character, MapData &map);
    void recolorCharacter(const SharedGroupChar &character, const QColor &newColor);

signals:
    void mapLoadedChanged();
    void sig_center(const glm::vec2 &pos);
    void sig_kickCharacter(const QString &name);

private:
    Mmapper2Group &m_group;
    bool m_mapLoaded = false;
};
