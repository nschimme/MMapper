#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2023 The MMapper Authors

#include "../src/global/macros.h"

#include <QObject>

class NODISCARD_QOBJECT TestCow final : public QObject
{
    Q_OBJECT
private slots:
    void testReadOnlySharing();
    void testLazyCopyOnWrite();
    void testMutationIsolation();
    void testFinalize();
    void testWorldCopyCOW();
    void testWorldApplyChangeCOW(); // Added a more specific world test
};
