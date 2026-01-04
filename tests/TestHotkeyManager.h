#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "../src/global/macros.h"

#include <QObject>

class NODISCARD_QOBJECT TestHotkeyManager final : public QObject
{
    Q_OBJECT

public:
    TestHotkeyManager();
    ~TestHotkeyManager() final;

private Q_SLOTS:
    // Setup/teardown for QSettings isolation
    void initTestCase();
    void cleanupTestCase();

    // Tests
    void keyNormalizationTest();
    void importExportRoundTripTest();
    void importEdgeCasesTest();
    void resetToDefaultsTest();
    void setHotkeyTest();
    void removeHotkeyTest();
    void directLookupTest();

private:
    // Original QSettings namespace (restored in cleanupTestCase)
    QString m_originalOrganization;
    QString m_originalApplication;
};
