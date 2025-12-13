#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "ConfigEnums.h"

#include <stdexcept>

#include <qprocessordetection.h>
#include <qsystemdetection.h>

#include "PackageType.h"

#include <string_view>

static inline constexpr PackageTypeEnum CURRENT_PACKAGE_TYPE = [] {
    using namespace std::string_view_literals;
#ifndef MMAPPER_PACKAGE_TYPE
#    error "Missing package type"
#endif
    constexpr auto type = MMAPPER_PACKAGE_TYPE;
    if (type == "Source"sv) {
        return PackageTypeEnum::Source;
    }
    if (type == "Deb"sv) {
        return PackageTypeEnum::Deb;
    }
    if (type == "Dmg"sv) {
        return PackageTypeEnum::Dmg;
    }
    if (type == "Exe"sv) {
        return PackageTypeEnum::Exe;
    }
    if (type == "AppImage"sv) {
        return PackageTypeEnum::AppImage;
    }
    if (type == "AppX"sv) {
        return PackageTypeEnum::AppX;
    }
    if (type == "Flatpak"sv) {
        return PackageTypeEnum::Flatpak;
    }
    if (type == "Snap"sv) {
        return PackageTypeEnum::Snap;
    }
    if (type == "Wasm"sv) {
        return PackageTypeEnum::Wasm;
    }
    throw std::runtime_error("unsupported package type");
}();

static inline constexpr const PlatformEnum CURRENT_PLATFORM = [] {
#if defined(Q_OS_WIN)
    return PlatformEnum::Windows;
#elif defined(Q_OS_MAC)
    return PlatformEnum::Mac;
#elif defined(Q_OS_LINUX)
    return PlatformEnum::Linux;
#elif defined(Q_OS_WASM)
    return PlatformEnum::Wasm;
#else
    throw std::runtime_error("unsupported platform");
#endif
}();

static inline constexpr const EnvironmentEnum CURRENT_ENVIRONMENT = [] {
#if Q_PROCESSOR_WORDSIZE == 4
    return EnvironmentEnum::Env32Bit;
#elif Q_PROCESSOR_WORDSIZE == 8
    return EnvironmentEnum::Env64Bit;
#else
    throw std::runtime_error("unsupported environment");
#endif
}();
