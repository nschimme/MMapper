#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#include "../global/macros.h"
#include <QObject>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

class NODISCARD_QOBJECT MapWindowViewModel final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(glm::vec2 scrollPos READ scrollPos WRITE setScrollPos NOTIFY scrollPosChanged)

public:
    explicit MapWindowViewModel(QObject *parent = nullptr);

    NODISCARD glm::vec2 scrollPos() const { return m_scrollPos; }
    void setScrollPos(const glm::vec2 &pos);

    void setMapRange(const glm::ivec3 &min, const glm::ivec3 &max);

    NODISCARD glm::vec2 scrollToWorld(const glm::ivec2 &scroll) const;
    NODISCARD glm::ivec2 worldToScroll(const glm::vec2 &world) const;

signals:
    void scrollPosChanged();

private:
    glm::vec2 m_scrollPos{0.f};
    glm::ivec3 m_min{0}, m_max{0};
};
