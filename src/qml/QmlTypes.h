#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

// Registers MMapper's C++-backed QML types with the "MMapper" QML module.
// Nothing in the shipped app currently instantiates any of these types from
// QML (the QOpenGLWindow-based MapCanvas/MapWindow remain the active map
// UI); this call just makes them available for the future QML shell.
//
// Called once from main() under MMAPPER_WITH_QML, before any QQmlEngine is
// created (qmlRegisterType() must run before the type is first referenced
// from QML, and the simplest way to guarantee that is to call it up front).
void registerMmQmlTypes();
