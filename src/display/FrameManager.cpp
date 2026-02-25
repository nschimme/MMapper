// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "FrameManager.h"

#include "../configuration/configuration.h"

#include <algorithm>

FrameManager::FrameManager(QObject *parent)
    : QObject(parent)
{
    updateMinFrameTime();
    setConfig().canvas.advanced.maximumFps.registerChangeCallback(m_configLifetime, [this]() {
        updateMinFrameTime();
    });

    m_heartbeatTimer.setSingleShot(true);
    m_heartbeatTimer.setTimerType(Qt::PreciseTimer);
    connect(&m_heartbeatTimer, &QTimer::timeout, this, &FrameManager::onHeartbeat);
}

void FrameManager::registerCallback(const Signal2Lifetime &lifetime, AnimationCallback callback)
{
    m_callbacks.push_back({lifetime.getObj(), std::move(callback)});
}

bool FrameManager::needsHeartbeat() const
{
    if (m_animating) {
        return true;
    }

    auto it = m_callbacks.begin();
    while (it != m_callbacks.end()) {
        if (auto shared = it->lifetime.lock()) {
            if (it->callback()) {
                return true;
            }
            ++it;
        } else {
            it = m_callbacks.erase(it);
        }
    }
    return false;
}

std::optional<FrameManager::Frame> FrameManager::beginFrame()
{
    const auto now = std::chrono::steady_clock::now();

    // Throttle: check if enough time has passed since the start of the last successful frame.
    if (m_lastUpdateTime.time_since_epoch().count() != 0) {
        const auto elapsed = now - m_lastUpdateTime;
        // We use a small tolerance (5ms) to avoid skipping frames due to minor timer jitter.
        // This is crucial to avoid missing VSync windows on high-refresh displays.
        if (elapsed + std::chrono::milliseconds(5) < m_minFrameTime) {
            return std::nullopt;
        }
    }

    // Deduplication: skip frame if nothing has changed and we aren't animating.
    if (!m_dirty && !needsHeartbeat()) {
        return std::nullopt;
    }
    m_dirty = false;

    // Calculate delta time
    float dt = 0.0f;
    if (m_lastUpdateTime.time_since_epoch().count() != 0) {
        const auto elapsed = now - m_lastUpdateTime;
        dt = std::chrono::duration<float>(elapsed).count();
    }
    m_lastUpdateTime = now;

    // Advance global time smoothly
    m_animationTime += dt;

    // Cap dt for simulation to match map movement during dragging and avoid quantization jitter.
    // Cap at 1.0s to avoid huge jumps after window focus loss or lag, while supporting low FPS.
    m_lastFrameDeltaTime = std::min(dt, 1.0f);

    return Frame(*this, m_lastFrameDeltaTime);
}

void FrameManager::setAnimating(bool value)
{
    if (m_animating == value) {
        return;
    }

    m_animating = value;

    if (m_animating && !m_heartbeatTimer.isActive()) {
        onHeartbeat();
    }
}

void FrameManager::onHeartbeat()
{
    if (!needsHeartbeat()) {
        m_animating = false;
        m_heartbeatTimer.stop();
        return;
    }

    emit sig_requestUpdate();

    if (needsHeartbeat()) {
        // We always schedule a fallback heartbeat in case sig_requestUpdate is ignored.
        // recordFramePainted will later refine this with a more accurate delay if a paint occurs.
        const auto delayMs = std::chrono::duration_cast<std::chrono::milliseconds>(m_minFrameTime)
                                 .count();
        m_heartbeatTimer.start(static_cast<int>(delayMs));
    }
}

std::chrono::nanoseconds FrameManager::getTimeUntilNextFrame() const
{
    if (m_lastUpdateTime.time_since_epoch().count() == 0) {
        return std::chrono::nanoseconds::zero();
    }

    const auto now = std::chrono::steady_clock::now();
    const auto elapsed = now - m_lastUpdateTime;
    // We use the same tolerance as beginFrame.
    if (elapsed + std::chrono::milliseconds(5) >= m_minFrameTime) {
        return std::chrono::nanoseconds::zero();
    }
    return m_minFrameTime - elapsed;
}

void FrameManager::recordFramePainted()
{
    // The previous frame just finished painting. Schedule the next one.
    if (needsHeartbeat()) {
        requestFrame();
    }
}

void FrameManager::requestFrame()
{
    m_dirty = true;

    const auto delay = getTimeUntilNextFrame();
    if (delay == std::chrono::nanoseconds::zero()) {
        // We are ready to paint now. Stop any pending timer and request an update.
        if (m_heartbeatTimer.isActive()) {
            m_heartbeatTimer.stop();
        }
        emit sig_requestUpdate();
        // Start a fallback timer in case sig_requestUpdate is ignored.
        if (needsHeartbeat()) {
            const auto delayMs
                = std::chrono::duration_cast<std::chrono::milliseconds>(m_minFrameTime).count();
            m_heartbeatTimer.start(static_cast<int>(delayMs));
        }
    } else {
        // Start or restart the timer with the accurate delay.
        // We truncate to the nearest millisecond to favor earlier frames.
        const int finalDelay = static_cast<int>(
            std::chrono::duration_cast<std::chrono::milliseconds>(delay).count());

        // Only restart the timer if it's not active or if the new delay is significantly different.
        // This avoids hammering the timer during high-frequency input like mouse moves.
        if (!m_heartbeatTimer.isActive()
            || std::abs(m_heartbeatTimer.remainingTime() - finalDelay) > 1) {
            m_heartbeatTimer.start(std::max(1, finalDelay));
        }
    }
}

void FrameManager::updateMinFrameTime()
{
    const float targetFps = getConfig().canvas.advanced.maximumFps.getFloat();
    m_minFrameTime = std::chrono::nanoseconds(
        static_cast<long long>(1000000000.0 / static_cast<double>(std::max(1.0f, targetFps))));
}
