// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "AnimationManager.h"

#include "../opengl/legacy/Legacy.h"
#include "../opengl/legacy/VBO.h"

void AnimationManager::init(UboManager &uboManager)
{
    m_uboManager = &uboManager;
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
    }

    auto elapsed = now - m_lastUpdateTime;
    m_lastFrameDeltaTime = std::chrono::duration<float>(elapsed).count();
    m_animationTime += m_lastFrameDeltaTime;
    m_lastUpdateTime = now;

    // Refresh internal struct for UBO
    m_frameData.time = glm::vec4(m_animationTime, m_lastFrameDeltaTime, 0.0f, 0.0f);

    if (m_uboManager) {
        m_uboManager->invalidate(Legacy::SharedVboEnum::TimeBlock);
    }
}

void AnimationManager::updateAndBind(Legacy::Functions &gl)
{
    if (m_uboManager) {
        m_uboManager->updateAndBind(gl, Legacy::SharedVboEnum::TimeBlock, m_frameData);
    }
}
