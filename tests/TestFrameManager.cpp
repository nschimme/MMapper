#include "TestFrameManager.h"
#include "../src/display/FrameManager.h"
#include "../src/configuration/configuration.h"
#include <chrono>
#include <thread>

void TestFrameManager::testTargetFps()
{
    // Initialize config
    setEnteredMain();
    auto &conf = setConfig().canvas.advanced.maximumFps;
    conf.setFloat(60.0f); // 16.66ms interval

    FrameManager fm;
    fm.setDirty();

    // Simulating 60 FPS. Interval should be ~16.6ms.
    // Start a frame.
    auto frame1 = fm.beginFrame();
    QVERIFY(frame1.has_value());

    // Simulate 10ms render time.
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    frame1.reset(); // Destroying frame records end of paint (in old code) or nothing (in new code)

    // Check if next frame is throttled.
    fm.setDirty();
    auto frame2 = fm.beginFrame();
    QVERIFY(!frame2.has_value()); // Still within 16.6ms from start of frame1

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    fm.setDirty();
    auto frame3 = fm.beginFrame();
    QVERIFY(frame3.has_value()); // Now it should be ~20ms since start of frame1, so it's OK.
}

void TestFrameManager::testThrottle()
{
    auto &conf = setConfig().canvas.advanced.maximumFps;
    conf.setFloat(4.0f);

    FrameManager fm;
    fm.setDirty();

    auto frame1 = fm.beginFrame();
    QVERIFY(frame1.has_value());

    // Even if paint is super fast (1ms)
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    frame1.reset();

    // 100ms later it should still be throttled.
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    fm.setDirty();
    auto frame2 = fm.beginFrame();
    QVERIFY(!frame2.has_value());

    // 255ms (total) later it should be OK.
    std::this_thread::sleep_for(std::chrono::milliseconds(155));
    fm.setDirty();
    auto frame3 = fm.beginFrame();
    QVERIFY(frame3.has_value());
}

void TestFrameManager::testDecoupling()
{
    auto &conf = setConfig().canvas.advanced.maximumFps;
    conf.setFloat(60.0f); // 16.66ms interval

    FrameManager fm;
    fm.setDirty();

    auto frame1 = fm.beginFrame();
    QVERIFY(frame1.has_value());

    // Simulate 12ms render time.
    std::this_thread::sleep_for(std::chrono::milliseconds(12));
    frame1.reset(); // End of frame at T=12ms

    // Total time elapsed since start of frame1: 12ms.
    // Next frame should be available at T=16.66ms.

    // Check it's still throttled at T=14ms
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    fm.setDirty();
    auto frame_fail = fm.beginFrame();
    QVERIFY(!frame_fail.has_value());

    // Wait until T=20ms
    std::this_thread::sleep_for(std::chrono::milliseconds(6));
    fm.setDirty();
    auto frame2 = fm.beginFrame();
    QVERIFY(frame2.has_value());
    // In old code, this would have failed because it waited 16.66ms AFTER frame1 finished (T=12 + 16.66 = 28.66ms).
}

#include "TestFrameManager.moc"
QTEST_MAIN(TestFrameManager)
