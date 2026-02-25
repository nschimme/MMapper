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
    fm.setDirty();

    // Start a frame.
    auto frame1 = fm.beginFrame();
    QVERIFY(frame1.has_value());

    // Wait a bit, but much less than 100ms.
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    frame1.reset();

    // Check if next frame is throttled.
    fm.setDirty();
    auto frame2 = fm.beginFrame();
    // At T=20ms (plus sleep jitter), it should be throttled by the 100ms limit.
    // 20ms + 5ms tolerance = 25ms < 100ms.
    QVERIFY(!frame2.has_value());

    // Total time elapsed since start of frame1 should be > 100ms now.
    std::this_thread::sleep_for(std::chrono::milliseconds(90));
    fm.setDirty();
    auto frame3 = fm.beginFrame();
    QVERIFY(frame3.has_value());
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
    conf.setFloat(5.0f); // 200ms interval

    FrameManager fm;
    fm.setDirty();

    auto frame1 = fm.beginFrame();
    QVERIFY(frame1.has_value());

    // Simulate 50ms render time.
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    frame1.reset(); // End of frame at T=50ms

    // Total time elapsed since start of frame1: 50ms.
    // Next frame should be available at T=200ms.

    // Check it's still throttled at T=100ms (50ms sleep + 50ms previous)
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    fm.setDirty();
    auto frame_fail = fm.beginFrame();
    QVERIFY(!frame_fail.has_value());

    // Wait until T=210ms (110ms more)
    std::this_thread::sleep_for(std::chrono::milliseconds(110));
    fm.setDirty();
    auto frame2 = fm.beginFrame();
    QVERIFY(frame2.has_value());
}

void TestFrameManager::testHammering()
{
    auto &conf = setConfig().canvas.advanced.maximumFps;
    conf.setFloat(4.0f); // 250ms interval

    FrameManager fm;
    fm.setDirty();

    auto frame1 = fm.beginFrame();
    QVERIFY(frame1.has_value());
    frame1.reset();

    // Call requestFrame many times (simulating mouse move)
    for (int i = 0; i < 50; ++i) {
        fm.requestFrame();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    // After 50ms, we should still be throttled.
    fm.setDirty();
    auto frame2 = fm.beginFrame();
    QVERIFY(!frame2.has_value());

    // Wait until 255ms from start
    std::this_thread::sleep_for(std::chrono::milliseconds(205));
    fm.setDirty();
    auto frame3 = fm.beginFrame();
    QVERIFY(frame3.has_value());
}

#include "TestFrameManager.moc"
QTEST_MAIN(TestFrameManager)
