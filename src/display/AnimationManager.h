#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "../global/RuleOf5.h"
#include "../global/Signal2.h"
#include "../opengl/UboManager.h"

#include <chrono>
#include <functional>
#include <memory>
#include <vector>

class AnimationManager final
{
public:
    using AnimationCallback = std::function<bool()>;

    AnimationManager() = default;
    DELETE_CTORS_AND_ASSIGN_OPS(AnimationManager);

public:
    void init(UboManager &uboManager);
    void registerCallback(const Signal2Lifetime &lifetime, AnimationCallback callback);
    NODISCARD bool isAnimating() const;

    /**
     * @brief Advance the global animation clock.
     * Calculates the delta time since the last call to update().
     */
    void update();

    void setAnimating(bool value) { m_animating = value; }
    NODISCARD bool getAnimating() const { return m_animating; }

    NODISCARD float getAnimationTime() const { return m_animationTime; }
    NODISCARD float getLastFrameDeltaTime() const { return m_lastFrameDeltaTime; }

private:
    struct Entry
    {
        std::weak_ptr<Signal2Lifetime::Obj> lifetime;
        AnimationCallback callback;
    };
    mutable std::vector<Entry> m_callbacks;

    std::chrono::steady_clock::time_point m_lastUpdateTime;
    bool m_animating = false;

    float m_animationTime = 0.0f;
    float m_lastFrameDeltaTime = 0.0f;
    UboManager *m_uboManager = nullptr;
};
