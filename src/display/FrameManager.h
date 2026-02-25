#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "../global/RuleOf5.h"
#include "../global/Signal2.h"

#include <chrono>
#include <functional>
#include <memory>
#include <vector>

#include <QObject>
#include <QTimer>

class FrameManager final : public QObject
{
    Q_OBJECT

public:
    using AnimationCallback = std::function<bool()>;

private:
    struct Entry
    {
        std::weak_ptr<Signal2Lifetime::Obj> lifetime;
        AnimationCallback callback;
    };

    mutable std::vector<Entry> m_callbacks;
    std::chrono::steady_clock::time_point m_lastUpdateTime;
    std::chrono::steady_clock::time_point m_lastPaintTime;
    std::chrono::nanoseconds m_minFrameTime{0};
    Signal2Lifetime m_configLifetime;
    QTimer m_heartbeatTimer;
    float m_animationTime = 0.0f;
    float m_lastFrameDeltaTime = 0.0f;
    bool m_animating = false;

public:
    explicit FrameManager(QObject *parent = nullptr);
    DELETE_CTORS_AND_ASSIGN_OPS(FrameManager);

public:
    void registerCallback(const Signal2Lifetime &lifetime, AnimationCallback callback);

    /**
     * @brief Check if any registered animations are active.
     */
    NODISCARD bool isAnimating() const;

    /**
     * @brief Advance the global animation clock.
     * Calculates the delta time since the last call to update().
     */
    void update();

    /**
     * @brief Enable or disable the background animation heartbeat.
     */
    void setAnimating(bool value);
    NODISCARD bool getAnimating() const { return m_animating; }

    /**
     * @brief Manually trigger an update if animating.
     */
    void requestUpdateIfAnimating();

    NODISCARD float getAnimationTime() const { return m_animationTime; }
    NODISCARD float getLastFrameDeltaTime() const { return m_lastFrameDeltaTime; }

    /**
     * @brief Check if enough time has passed to render a new frame based on the FPS limit.
     */
    NODISCARD bool tryAcquireFrame() const;

    /**
     * @brief Get the duration to wait until the next frame can be rendered.
     */
    NODISCARD std::chrono::nanoseconds getTimeUntilNextFrame() const;

    /**
     * @brief Record that a frame was successfully painted.
     */
    void recordFramePainted();

    /**
     * @brief Request a frame to be painted, respecting the FPS limit.
     * Emits sig_requestUpdate() immediately or schedules a future heartbeat.
     */
    void requestFrame();

private:
    void updateMinFrameTime();

signals:
    void sig_requestUpdate();

private slots:
    void onHeartbeat();
};
