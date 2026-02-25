// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "TestFrameManager.h"

#include "../src/configuration/configuration.h"
#include "../src/display/FrameManager.h"

#include <chrono>
#include <thread>

void TestFrameManager::testTargetFps()
{
    // Initialize config
    setEnteredMain();
    auto &conf = setConfig().canvas.advanced.maximumFps;
    conf.setFloat(10.0f); // 100ms interval

    FrameManager fm;
    fm.invalidate();

    // Start a frame.
    const auto t1 = std::chrono::steady_clock::now();
    auto frame1 = fm.beginFrame();
    QVERIFY(frame1.has_value());

    // Wait a bit, but much less than 100ms.
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    frame1.reset();

    // Check if next frame is throttled.
    fm.invalidate();
    const auto t2 = std::chrono::steady_clock::now();
    auto frame2 = fm.beginFrame();
    const auto elapsed = t2 - t1;
    // Throttling depends on the actual elapsed time. If sleep took too long, it might not be throttled.
    // We use a margin (25ms) in the test to be robust against CI jitter.
    if (elapsed + std::chrono::milliseconds(25) < std::chrono::milliseconds(100)) {
        QVERIFY(!frame2.has_value());
    }

    // Total time elapsed since start of frame1 should be > 100ms now.
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    fm.invalidate();
    auto frame3 = fm.beginFrame();
    QVERIFY(frame3.has_value());
}

void TestFrameManager::testThrottle()
{
    auto &conf = setConfig().canvas.advanced.maximumFps;
    conf.setFloat(4.0f);

    FrameManager fm;
    fm.invalidate();

    auto frame1 = fm.beginFrame();
    QVERIFY(frame1.has_value());

    // Even if paint is super fast (1ms)
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    frame1.reset();

    // 100ms later it should still be throttled.
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    fm.invalidate();
    auto frame2 = fm.beginFrame();
    QVERIFY(!frame2.has_value());

    // 255ms (total) later it should be OK.
    std::this_thread::sleep_for(std::chrono::milliseconds(155));
    fm.invalidate();
    auto frame3 = fm.beginFrame();
    QVERIFY(frame3.has_value());
}

void TestFrameManager::testDecoupling()
{
    auto &conf = setConfig().canvas.advanced.maximumFps;
    conf.setFloat(5.0f); // 200ms interval

    FrameManager fm;
    fm.invalidate();

    const auto t1 = std::chrono::steady_clock::now();
    auto frame1 = fm.beginFrame();
    QVERIFY(frame1.has_value());

    // Simulate 50ms render time.
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    frame1.reset(); // End of frame at T=50ms

    // Check it's still throttled at T=100ms
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    fm.invalidate();
    const auto t2 = std::chrono::steady_clock::now();
    auto frame_fail = fm.beginFrame();
    const auto elapsed = t2 - t1;
    if (elapsed + std::chrono::milliseconds(25) < std::chrono::milliseconds(200)) {
        QVERIFY(!frame_fail.has_value());
    }

    // Wait until T=210ms
    std::this_thread::sleep_for(std::chrono::milliseconds(110));
    fm.invalidate();
    auto frame2 = fm.beginFrame();
    QVERIFY(frame2.has_value());
}

void TestFrameManager::testHammering()
{
    auto &conf = setConfig().canvas.advanced.maximumFps;
    conf.setFloat(4.0f); // 250ms interval

    FrameManager fm;
    fm.invalidate();

    const auto t1 = std::chrono::steady_clock::now();
    auto frame1 = fm.beginFrame();
    QVERIFY(frame1.has_value());
    frame1.reset();

    // Call requestFrame many times (simulating mouse move)
    for (int i = 0; i < 50; ++i) {
        fm.requestFrame();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    // After some time, we should still be throttled.
    fm.invalidate();
    const auto t2 = std::chrono::steady_clock::now();
    auto frame2 = fm.beginFrame();
    const auto elapsed = t2 - t1;
    if (elapsed + std::chrono::milliseconds(25) < std::chrono::milliseconds(250)) {
        QVERIFY(!frame2.has_value());
    }

    // Wait until 260ms from start
    std::this_thread::sleep_for(std::chrono::milliseconds(210));
    fm.invalidate();
    auto frame3 = fm.beginFrame();
    QVERIFY(frame3.has_value());
}

QTEST_MAIN(TestFrameManager)
