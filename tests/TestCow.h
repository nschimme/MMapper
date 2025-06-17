#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2023 The MMapper Authors

#include <QObject>

class TestCow : public QObject {
    Q_OBJECT

public:
    TestCow() = default;
    ~TestCow() = default;

private slots:
    void testReadOnlySharing();
    void testLazyCopyOnWrite();
    void testMutationIsolation();
    void testFinalize();
    void testWorldCopyCOW();
    void testWorldApplyChangeCOW(); // Added a more specific world test
};
