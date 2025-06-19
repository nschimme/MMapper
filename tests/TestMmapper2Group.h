#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2023 The MMapper Authors

#include "../src/global/macros.h"
#include <QObject>
#include <memory> // For std::unique_ptr

// Forward declarations
class Mmapper2Group;
class Configuration;
class CGroupChar; // For checking character properties
// Required for SharedGroupChar type used in the helper
#include "../src/group/CGroupChar.h"


class NODISCARD_QOBJECT TestMmapper2Group final : public QObject
{
    Q_OBJECT

public:
    TestMmapper2Group();
    ~TestMmapper2Group() final;

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    void testSettingsChangeTriggersSlot();
    void testSlotEmitsUpdateWidgetSignal();
    void testPlayerColorSettingApplied();
    void testNpcColorSettingApplied(); // With override true and false

private:
    Mmapper2Group *m_mapper2Group = nullptr;
    // Helper to add a character for testing - might need GroupManagerApi interaction
    // For simplicity, if Mmapper2Group has internal addChar or similar for test setup
    SharedGroupChar addTestCharToM2G(const QString& name, bool isPlayer, bool isNpc, const QString& initialColorName = "");
};
