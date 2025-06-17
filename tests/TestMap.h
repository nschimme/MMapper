#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2024 The MMapper Authors

#include "../src/global/macros.h"

#include <QObject>

class NODISCARD_QOBJECT TestMap final : public QObject
{
    Q_OBJECT

public:
    TestMap();
    ~TestMap() final;

private Q_SLOTS:
    static void diffTest();
    static void mapTest();
    static void sanitizerTest();
    static void tinyRoomIdSetTest();
    static void spatialDbCowTest();
    // Slots from TestCow
    static void testReadOnlySharing();
    static void testLazyCopyOnWrite();
    static void testMutationIsolation();
    static void testFinalize();
    static void testWorldCopyCOW();
    static void testWorldApplyChangeCOW();
};
