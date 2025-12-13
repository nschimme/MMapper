// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2023 The MMapper Authors

#pragma once

#include <string_view>

enum class PackageTypeEnum {
    Source,
    Deb,
    Dmg,
    Exe,
    AppImage,
    AppX,
    Flatpak,
    Snap,
    Wasm
};
