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

    class Frame final
    {
    public:
        explicit Frame(FrameManager &manager, float dt)
            : m_manager(manager)
            , m_dt(dt)
        {}
        ~Frame()
        {
            if (m_active) {
                m_manager.recordFramePainted();
            }
        }
        Frame(Frame &&other) noexcept
            : m_manager(other.m_manager)
            , m_dt(other.m_dt)
            , m_active(std::exchange(other.m_active, false))
        {}
        DELETE_COPIES(Frame);
        DELETE_MOVE_ASSIGN_OP(Frame);

    public:
        NODISCARD float dt() const { return m_dt; }

    private:
        FrameManager &m_manager;
        float m_dt;
        bool m_active = true;
    };

private:
    struct Entry
    {
        std::weak_ptr<Signal2Lifetime::Obj> lifetime;
        AnimationCallback callback;
    };

    mutable std::vector<Entry> m_callbacks;
    /** @brief The start time of the last successful frame. Used for dt and throttling. */
    std::chrono::steady_clock::time_point m_lastUpdateTime;
    std::chrono::nanoseconds m_minFrameTime{0};
    Signal2Lifetime m_configLifetime;
    QTimer m_heartbeatTimer;
    float m_animationTime = 0.0f;
    float m_lastFrameDeltaTime = 0.0f;
    bool m_animating = false;
    bool m_dirty = true;

public:
    explicit FrameManager(QObject *parent = nullptr);
    DELETE_CTORS_AND_ASSIGN_OPS(FrameManager);

public:
    void registerCallback(const Signal2Lifetime &lifetime, AnimationCallback callback);

    /**
     * @brief Check if enough time has passed to render a new frame.
     * Returns a Frame object if successful, which handles recording upon destruction.
     */
    NODISCARD std::optional<Frame> beginFrame();

    /**
     * @brief Request a frame to be painted.
     * Respects FPS limit and avoids redundant updates if not dirty.
     */
    void requestFrame();

    /**
     * @brief Mark the view state as dirty, ensuring the next requested frame isn't skipped.
     */
    void setDirty() { m_dirty = true; }

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
    NODISCARD std::chrono::nanoseconds getTimeUntilNextFrame() const;

signals:
    void sig_requestUpdate();

private slots:
    void onHeartbeat();
};
