#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "../src/global/macros.h"

#include <QObject>

class NODISCARD_QOBJECT TestQml final : public QObject
{
    Q_OBJECT

public:
    TestQml();
    ~TestQml() final;

private Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();
    void loadPanelFrame();
    void loadPanelHeaderRow();
    void qmlDockWidgetFallback();
    void qmlDockWidgetLoads();
    void loadTimerPanel();
    void loadAdventurePanel();
    void roomModelLongestTextInColumn();
    void loadRoomPanel();
    void loadLogPanel();
    void logModelCap();
    void qmlConfigRoundTrip();
    void loadGroupPanel();
    void loadDescriptionPanel();
    void descriptionPanelBlurVisible();
    void descriptionAdapterRealResolution();
    void descriptionPanelRealResolutionRenders();
    void tasksModelEmpty();
    void tasksModelHoldRemovalsRoundTrip();
    void tasksModelLifecycle();
    void loadTasksPanel();
    void loadXpStatusItem();
    void loadClockStrip();
    void clockAdapterNativeToolTip();
    void loadClientDisplay();
    void clientDisplayLeadingWhitespaceRenders();
    void clientDisplayStickTracking();
    void loadClientPanel();
};
