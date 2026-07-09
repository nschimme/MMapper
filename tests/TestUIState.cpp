// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2024 The MMapper Authors

#include "TestUIState.h"

#include "../src/display/mapcanvas.h"
#include "../src/display/prespammedpath.h"
#include "../src/map/coordinate.h"

#include <QtTest/QtTest>

TestUIState::TestUIState() = default;
TestUIState::~TestUIState() = default;

void TestUIState::initTestCase() {}
void TestUIState::cleanupTestCase() {}

void TestUIState::mouseSelTest()
{
    Coordinate2f pos(0.5f, 0.5f);
    int layer = 2;
    MouseSel sel(pos, layer);

    QCOMPARE(sel.layer, 2);
    QCOMPARE(sel.pos.x, 0.5f);
    QCOMPARE(sel.pos.y, 0.5f);

    Coordinate c = sel.getCoordinate();
    QCOMPARE(c.z, 2);
}

void TestUIState::inputStateInteractionTest()
{
    PrespammedPath prespammedPath(nullptr);
    MapCanvasInputState state(prespammedPath);
    QVERIFY(!state.m_activeInteraction.has_value());

    state.beginConnectionInteraction();
    QVERIFY(state.m_activeInteraction.has_value());
    QVERIFY(state.hasConnectionInteraction());

    state.endInteraction();
    QVERIFY(!state.m_activeInteraction.has_value());
}

void TestUIState::inputStatePanningTest()
{
    PrespammedPath prespammedPath(nullptr);
    MapCanvasInputState state(prespammedPath);
    QVERIFY(!state.m_panningState.has_value());

    glm::vec3 worldPos(0.f);
    glm::vec2 scroll(0.f);
    glm::mat4 viewProj(1.f);
    glm::vec2 screenPos(0.f);
    state.beginPanning(worldPos, scroll, viewProj, screenPos);
    QVERIFY(state.m_panningState.has_value());

    state.endPanning();
    QVERIFY(!state.m_panningState.has_value());
}

void TestUIState::inputStateGestureTest()
{
    PrespammedPath prespammedPath(nullptr);
    MapCanvasInputState state(prespammedPath);
    QVERIFY(!state.m_pinchState.has_value());
    QVERIFY(!state.m_magnificationState.has_value());

    state.beginPinch(100.0f);
    QVERIFY(state.m_pinchState.has_value());

    state.endPinch();
    QVERIFY(!state.m_pinchState.has_value());

    state.beginMagnification();
    QVERIFY(state.m_magnificationState.has_value());

    state.endMagnification();
    QVERIFY(!state.m_magnificationState.has_value());
}

QTEST_MAIN(TestUIState)
