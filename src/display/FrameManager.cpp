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

    // Throttle: check if enough time has passed since the last paint.
    if (m_lastPaintTime.time_since_epoch().count() != 0) {
        const auto elapsed = now - m_lastPaintTime;
        // We use a small tolerance (500us) to avoid skipping frames due to minor timer jitter.
        if (elapsed + std::chrono::microseconds(500) < m_minFrameTime) {
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
    // Cap at 0.1s to avoid huge jumps after window focus loss or lag.
    m_lastFrameDeltaTime = std::min(dt, 0.1f);

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
        const auto delay = getTimeUntilNextFrame();
        // Ensure we wait at least 1ms if we just rendered, or more if we are still under the throttle.
        const long long delayMs = std::chrono::duration_cast<std::chrono::milliseconds>(delay)
                                      .count();
        const bool hasPartialMs = delay > std::chrono::milliseconds(delayMs);
        const long long finalDelay = std::max(1LL, delayMs + (hasPartialMs ? 1LL : 0LL));
        m_heartbeatTimer.start(static_cast<int>(finalDelay));
    }
}

std::chrono::nanoseconds FrameManager::getTimeUntilNextFrame() const
{
    if (m_lastPaintTime.time_since_epoch().count() == 0) {
        return std::chrono::nanoseconds::zero();
    }

    const auto now = std::chrono::steady_clock::now();
    const auto elapsed = now - m_lastPaintTime;
    if (elapsed >= m_minFrameTime) {
        return std::chrono::nanoseconds::zero();
    }
    return m_minFrameTime - elapsed;
}

void FrameManager::recordFramePainted()
{
    m_lastPaintTime = std::chrono::steady_clock::now();
}

void FrameManager::requestFrame()
{
    m_dirty = true;
    if (m_heartbeatTimer.isActive()) {
        return;
    }

    const auto delay = getTimeUntilNextFrame();
    if (delay == std::chrono::nanoseconds::zero()) {
        emit sig_requestUpdate();
    } else {
        const long long delayMs = std::chrono::duration_cast<std::chrono::milliseconds>(delay)
                                      .count();
        const bool hasPartialMs = delay > std::chrono::milliseconds(delayMs);
        const long long finalDelay = delayMs + (hasPartialMs ? 1LL : 0LL);
        m_heartbeatTimer.start(static_cast<int>(finalDelay));
    }
}

void FrameManager::updateMinFrameTime()
{
    const float targetFps = getConfig().canvas.advanced.maximumFps.getFloat();
    m_minFrameTime = std::chrono::nanoseconds(
        static_cast<long long>(1000000000.0 / static_cast<double>(std::max(1.0f, targetFps))));
}
