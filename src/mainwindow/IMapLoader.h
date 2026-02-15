#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2024 The MMapper Authors

#include <memory>

#include <QString>

class MapSource;

class IMapLoader
{
public:
    virtual ~IMapLoader() = default;
    virtual void loadFile(std::shared_ptr<MapSource> source) = 0;
};
