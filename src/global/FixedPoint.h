#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "ChangeMonitor.h"
#include "RuleOf5.h"
#include "utils.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <QSettings>
#include <QSlider>
#include <QVariant>

template<int Digits_>
class NODISCARD FixedPoint final
{
private:
    ChangeMonitor m_changeMonitor;
    bool m_notifying = false;
    std::string m_key;
    std::string m_label;

public:
    static constexpr const int digits = Digits_;
    const int min;
    const int max;
    const int defaultValue;

private:
    int m_value = 0;

private:
    explicit FixedPoint(const int min_,
                        const int max_,
                        const int defaultValue_,
                        const int value,
                        std::string key = "",
                        std::string label = "")
        : m_key{std::move(key)}
        , m_label{std::move(label)}
        , min{min_}
        , max{max_}
        , defaultValue{defaultValue_}
        , m_value{std::clamp(value, min_, max_)}
    {
        // set(value);
        static_assert(0 <= digits && digits < 6);
        if (min_ > max_) {
            throw std::invalid_argument("min/max");
        }
        if (defaultValue_ < min_ || defaultValue_ > max_) {
            throw std::invalid_argument("defaultValue");
        }
    }

public:
    explicit FixedPoint(const int min_, const int max_, const int defaultValue_)
        : FixedPoint(min_, max_, defaultValue_, defaultValue_)
    {}

    explicit FixedPoint(
        std::string key, std::string label, const int min_, const int max_, const int defaultValue_)
        : FixedPoint(min_, max_, defaultValue_, defaultValue_, std::move(key), std::move(label))
    {}

    FixedPoint(const FixedPoint &other)
        : FixedPoint(other.min,
                     other.max,
                     other.defaultValue,
                     other.m_value,
                     other.m_key,
                     other.m_label)
    {}

    FixedPoint &operator=(const FixedPoint &other)
    {
        if (this != &other) {
            set(other.m_value);
        }
        return *this;
    }

    ~FixedPoint() = default;

public:
    NODISCARD bool operator==(const FixedPoint &other) const { return m_value == other.m_value; }
    NODISCARD bool operator!=(const FixedPoint &other) const { return !(*this == other); }

public:
    void reset() { set(defaultValue); }
    void set(const int value)
    {
        if (m_notifying) {
            throw std::runtime_error("recursion");
        }

        const int newValue = std::clamp(value, min, max);
        if (m_value == newValue) {
            return;
        }

        struct NODISCARD NotificationGuard final
        {
            FixedPoint &m_self;
            NotificationGuard() = delete;
            DELETE_CTORS_AND_ASSIGN_OPS(NotificationGuard);
            explicit NotificationGuard(FixedPoint &self)
                : m_self{self}
            {
                if (m_self.m_notifying) {
                    throw std::runtime_error("recursion");
                }
                m_self.m_notifying = true;
            }

            ~NotificationGuard()
            {
                assert(m_self.m_notifying);
                m_self.m_notifying = false;
            }
        } notification_guard{*this};

        m_value = newValue;
        m_changeMonitor.notifyAll();
    }

    void setFloat(const float f)
    {
        if (!std::isfinite(f)) {
            throw std::invalid_argument("f");
        }
        return set(static_cast<int>(std::lround(f * std::pow(10.f, static_cast<float>(digits)))));
    }

    NODISCARD int get() const { return m_value; }
    NODISCARD float getFloat() const
    {
        return static_cast<float>(get()) * std::pow(10.f, -static_cast<float>(digits));
    }
    NODISCARD double getDouble() const
    {
        return static_cast<double>(get()) * std::pow(10.0, -static_cast<double>(digits));
    }

public:
    // NOTE: The clone does not inherit our change monitor!
    NODISCARD FixedPoint clone(int value) const
    {
        return FixedPoint<Digits_>{min, max, defaultValue, value, m_key, m_label};
    }

    NODISCARD const std::string &getKey() const { return m_key; }
    NODISCARD const std::string &getLabel() const { return m_label; }

    void read(const QSettings &settings)
    {
        if (m_key.empty()) {
            return;
        }
        const QVariant var = settings.value(QString::fromStdString(m_key), defaultValue);
        set(var.toInt());
    }

    void write(QSettings &settings) const
    {
        if (m_key.empty()) {
            return;
        }
        settings.setValue(QString::fromStdString(m_key), m_value);
    }

public:
    void registerChangeCallback(const ChangeMonitor::Lifetime &lifetime,
                                ChangeMonitor::Function callback) const
    {
        return m_changeMonitor.registerChangeCallback(lifetime, std::move(callback));
    }
};
