// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "AnimationManager.h"
#include "../opengl/legacy/Legacy.h"
#include "../opengl/legacy/VBO.h"

void AnimationManager::init(UboManager &uboManager)
{
    m_uboManager = &uboManager;
    m_uboManager->registerUbo(Legacy::SharedVboEnum::TimeBlock, [this](Legacy::Functions &gl_funcs) {
        GLRenderState::Uniforms::Weather::Frame f;
        f.time = glm::vec4(m_animationTime, m_lastDt, 0.0f, 0.0f);
        const auto shared = gl_funcs.getSharedVbos().get(Legacy::SharedVboEnum::TimeBlock);
        std::ignore = gl_funcs.setUbo(shared->get(),
                                      std::vector<GLRenderState::Uniforms::Weather::Frame>{f},
                                      BufferUsageEnum::DYNAMIC_DRAW);
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

void AnimationManager::update(float dt)
{
    m_lastDt = dt;
    m_animationTime += dt;
    if (m_uboManager) {
        m_uboManager->invalidate(Legacy::SharedVboEnum::TimeBlock);
    }
}
