#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "../global/RAII.h"
#include "../global/RuleOf5.h"
#include "../global/Signal2.h"

#include <chrono>
#include <functional>
#include <memory>
#include <vector>

#include <QObject>
#include <QTimer>

/**
 * @brief Manages the application's frame-rate and animation lifecycle.
 *
 * FrameManager provides:
 * 1. Pacing: Ensures the application doesn't exceed the target FPS limit.
 * 2. Delta Time: Calculates accurate time between frames for smooth animations.
 * 3. Heartbeat: Automatically requests new frames while animations are active.
 * 4. Dirty Tracking: Skips redundant renders when nothing has changed.
 *
 * It uses a "start-to-start" interval pacing approach, which ensures the target
 * frame rate is hit regardless of how long each frame takes to render (as long
 * as it's within the frame window).
 */
class FrameManager final : public QObject
{
    Q_OBJECT

public:
    enum class AnimationStatus { Continue, Stop };
    using AnimationCallback = std::function<AnimationStatus()>;

private:
    struct Entry
    {
        std::weak_ptr<Signal2Lifetime::Obj> lifetime;
        AnimationCallback callback;
    };

    mutable std::vector<Entry> m_callbacks;
    std::chrono::steady_clock::time_point m_lastUpdateTime;
    std::chrono::nanoseconds m_minFrameTime{0};
    Signal2Lifetime m_configLifetime;
    QTimer m_heartbeatTimer;
    float m_animationTime = 0.0f;
    float m_lastFrameDeltaTime = 0.0f;
    bool m_animating = false;
    bool m_dirty = true;

public:
    struct Pacing
    {
        /** @brief How much early a frame is allowed to start to accommodate timer jitter. */
        static constexpr std::chrono::milliseconds JitterTolerance{8};
        /** @brief Lead-in time when scheduling the heartbeat timer to ensure we are "ready early". */
        static constexpr std::chrono::milliseconds TimerLeadIn{2};
    };

    /**
     * @brief RAII object representing a successful frame start.
     *
     * On destruction, it automatically notifies the manager that the frame
     * was painted, allowing it to schedule the next heartbeat.
     */
    class Frame final
    {
    public:
        explicit Frame(FrameManager &manager, float dt)
            : m_dt(dt)
            , m_callback([&manager]() { manager.recordFramePainted(); })
        {}

        DELETE_COPIES(Frame);
        DEFAULT_MOVE_CTOR(Frame);
        DELETE_MOVE_ASSIGN_OP(Frame);

    public:
        NODISCARD float dt() const { return m_dt; }

    private:
        float m_dt;
        RAIICallback m_callback;
    };

public:
    explicit FrameManager(QObject *parent = nullptr);
    DELETE_CTORS_AND_ASSIGN_OPS(FrameManager);

public:
    /**
     * @brief Register a callback that will be executed once per heartbeat.
     * The heartbeat continues as long as any callback returns AnimationStatus::Continue.
     */
    void registerCallback(const Signal2Lifetime &lifetime, AnimationCallback callback);

    /**
     * @brief Mark the view state as dirty and request a frame.
     */
    void invalidate();

    /**
     * @brief Check if enough time has passed to render a new frame.
     * Returns a Frame object if successful, which handles recording upon destruction.
     */
    NODISCARD std::optional<Frame> beginFrame();

    /**
     * @brief Request a frame to be painted if not already throttled.
     * Does not set the dirty flag.
     */
    void requestFrame();

    /**
     * @brief Enable or disable continuous background animation.
     */
    void setAnimating(bool value);
    NODISCARD bool getAnimating() const { return m_animating; }

    NODISCARD float getAnimationTime() const { return m_animationTime; }

    /**
     * @brief Check if any registered animations or heartbeat are active.
     */
    NODISCARD bool needsHeartbeat() const;

private:
    void recordFramePainted();
    void updateMinFrameTime();
    void cleanupExpiredCallbacks() const;
    NODISCARD std::chrono::nanoseconds getTimeUntilNextFrame() const;

signals:
    void sig_requestUpdate();

private slots:
    void onHeartbeat();
};
