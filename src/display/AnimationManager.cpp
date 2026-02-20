// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "AnimationManager.h"

#include "../opengl/legacy/Legacy.h"
#include "../opengl/legacy/VBO.h"

void AnimationManager::init(Legacy::UboManager &uboManager)
{
    m_uboManager = &uboManager;
    m_uboManager->registerRebuildFunction(Legacy::SharedVboEnum::TimeBlock,
                                          [this](Legacy::Functions &gl) {
                                              m_uboManager->update(gl,
                                                                   Legacy::SharedVboEnum::TimeBlock,
                                                                   m_frameData);
                                          });
}

void AnimationManager::registerCallback(const Signal2Lifetime &lifetime, AnimationCallback callback)
{
    m_callbacks.push_back({lifetime.getObj(), std::move(callback)});
}

bool AnimationManager::isAnimating() const
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

void AnimationManager::update()
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

    // Refresh internal struct for UBO
    m_frameData.time = glm::vec4(m_animationTime, m_lastFrameDeltaTime, 0.0f, 0.0f);

    if (m_uboManager) {
        m_uboManager->invalidate(Legacy::SharedVboEnum::TimeBlock);
    }
}
