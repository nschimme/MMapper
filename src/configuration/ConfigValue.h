#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "../global/ChangeMonitor.h"
#include "../global/RuleOf5.h"
#include "../global/utils.h"

#include <stdexcept>
#include <utility>

template<typename T>
class NODISCARD ConfigValue final
{
private:
    T m_value{};
    ChangeMonitor m_changeMonitor;
    bool m_notifying = false;

public:
    ConfigValue() = default;

    explicit ConfigValue(T initialValue)
        : m_value{std::move(initialValue)}
    {}

    ConfigValue(const ConfigValue &other)
        : m_value{other.m_value}
    {}

    ConfigValue &operator=(const ConfigValue &other)
    {
        set(other.m_value);
        return *this;
    }

    ConfigValue &operator=(const T &newValue)
    {
        set(newValue);
        return *this;
    }

    NODISCARD bool operator==(const ConfigValue &other) const
    {
        return utils::equals(m_value, other.m_value);
    }
    NODISCARD bool operator!=(const ConfigValue &other) const { return !(*this == other); }

    NODISCARD bool operator==(const T &otherValue) const
    {
        return utils::equals(m_value, otherValue);
    }
    NODISCARD bool operator!=(const T &otherValue) const { return !(*this == otherValue); }

    NODISCARD const T &get() const { return m_value; }
    NODISCARD operator T() const { return m_value; }
    NODISCARD const T *operator->() const { return &m_value; }

    void set(const T &newValue)
    {
        if (m_notifying) {
            throw std::runtime_error("recursion");
        }

        if (utils::equals(m_value, newValue)) {
            return;
        }

        struct NODISCARD NotificationGuard final
        {
            bool &m_flag;
            explicit NotificationGuard(bool &flag)
                : m_flag{flag}
            {
                m_flag = true;
            }
            ~NotificationGuard() { m_flag = false; }
        } guard{m_notifying};

        m_value = newValue;
        m_changeMonitor.notifyAll();
    }

    void registerChangeCallback(const ChangeMonitor::Lifetime &lifetime,
                                const ChangeMonitor::Function &callback)
    {
        m_changeMonitor.registerChangeCallback(lifetime, callback);
    }
};
