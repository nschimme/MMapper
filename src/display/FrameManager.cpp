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
    connect(&m_heartbeatTimer, &QTimer::timeout, this, &FrameManager::onHeartbeat);
}

void FrameManager::registerCallback(const Signal2Lifetime &lifetime, AnimationCallback callback)
{
    m_callbacks.push_back({lifetime.getObj(), std::move(callback)});
}

bool FrameManager::isAnimating() const
{
    bool anyAnimating = false;
    auto it = m_callbacks.begin();
    while (it != m_callbacks.end()) {
        if (auto shared = it->lifetime.lock()) {
            if (it->callback()) {
                anyAnimating = true;
            }
            ++it;
        } else {
            it = m_callbacks.erase(it);
        }
    }
    return anyAnimating;
}

void FrameManager::update()
{
    auto now = std::chrono::steady_clock::now();
    if (m_lastUpdateTime.time_since_epoch().count() == 0) {
        m_lastUpdateTime = now;
        return;
    }

    auto elapsed = now - m_lastUpdateTime;
    float dt = std::chrono::duration<float>(elapsed).count();
    m_lastUpdateTime = now;

    // Advance global time smoothly
    m_animationTime += dt;

    // Use raw dt for simulation to match map movement during dragging and avoid quantization jitter.
    // Cap at 0.1s to avoid huge jumps after window focus loss or lag.
    m_lastFrameDeltaTime = std::min(dt, 0.1f);
}

void FrameManager::setAnimating(bool value)
{
    if (m_animating == value) {
        return;
    }

    m_animating = value;

    if (m_animating) {
        onHeartbeat();
    } else {
        m_heartbeatTimer.stop();
    }
}

void FrameManager::requestUpdateIfAnimating()
{
    if (m_animating && !m_heartbeatTimer.isActive()) {
        onHeartbeat();
    }
}

void FrameManager::onHeartbeat()
{
    if (m_animating && !isAnimating()) {
        m_animating = false;
        return;
    }

    emit sig_requestUpdate();

    if (m_animating) {
        auto delay = getTimeUntilNextFrame();
        if (delay == std::chrono::nanoseconds::zero()) {
            delay = m_minFrameTime;
        }

        m_heartbeatTimer.start(std::chrono::duration_cast<std::chrono::milliseconds>(delay));
    }
}

bool FrameManager::tryAcquireFrame() const
{
    // Always allow the first frame to be painted to avoid a black screen on load.
    if (m_lastPaintTime.time_since_epoch().count() == 0) {
        return true;
    }

    auto now = std::chrono::steady_clock::now();
    auto elapsed = now - m_lastPaintTime;
    return elapsed >= m_minFrameTime;
}

std::chrono::nanoseconds FrameManager::getTimeUntilNextFrame() const
{
    auto now = std::chrono::steady_clock::now();
    if (m_lastPaintTime.time_since_epoch().count() == 0) {
        return std::chrono::nanoseconds::zero();
    }

    auto elapsed = now - m_lastPaintTime;
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
    if (tryAcquireFrame()) {
        emit sig_requestUpdate();
    } else if (!m_heartbeatTimer.isActive()) {
        const auto delay = getTimeUntilNextFrame();
        m_heartbeatTimer.start(std::chrono::duration_cast<std::chrono::milliseconds>(delay));
    }
}

void FrameManager::updateMinFrameTime()
{
    const float targetFps = getConfig().canvas.advanced.maximumFps.getFloat();
    m_minFrameTime = std::chrono::nanoseconds(
        static_cast<long long>(1000000000.0 / static_cast<double>(std::max(1.0f, targetFps))));
}
