#pragma once

#include <QtTest/QtTest>

class TestFrameManager : public QObject
{
    Q_OBJECT

public:
    TestFrameManager() = default;
    ~TestFrameManager() override = default;

private slots:
    void testTargetFps();
    void testThrottle();
    void testDecoupling();
    void testHammering();
};
