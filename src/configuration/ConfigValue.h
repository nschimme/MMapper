#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "../global/ChangeMonitor.h"
#include "../global/Color.h"
#include "../global/RuleOf5.h"
#include "../global/utils.h"

#include <algorithm>
#include <functional>
#include <string>
#include <utility>

#include <QColor>
#include <QSettings>
#include <QVariant>

template<typename T>
class NODISCARD ConfigValue final
{
private:
    std::string m_key;
    std::string m_label;
    T m_value{};
    T m_defaultValue{};
    ChangeMonitor m_changeMonitor;
    bool m_notifying = false;

public:
    using Validator = std::function<T(const T &)>;
    Validator m_validator;

public:
    ConfigValue() = default;

    explicit ConfigValue(T initialValue)
        : m_value{initialValue}
        , m_defaultValue{initialValue}
    {}

    ConfigValue(std::string key,
                std::string label,
                const T &defaultValue,
                Validator validator = nullptr)
        : m_key(std::move(key))
        , m_label(std::move(label))
        , m_value(defaultValue)
        , m_defaultValue(defaultValue)
        , m_validator(std::move(validator))
    {}

    ConfigValue(const ConfigValue &other)
        : m_key{other.m_key}
        , m_label{other.m_label}
        , m_value{other.m_value}
        , m_defaultValue{other.m_defaultValue}
        , m_validator{other.m_validator}
    {}

    ConfigValue &operator=(const ConfigValue &other)
    {
        if (this != &other) {
            set(other.m_value);
        }
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

    NODISCARD const std::string &getKey() const { return m_key; }
    NODISCARD const std::string &getLabel() const { return m_label; }
    NODISCARD const T &getDefaultValue() const { return m_defaultValue; }

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

    void reset() { set(m_defaultValue); }

    void clamp(const T &lo, const T &hi) { set(std::clamp(m_value, lo, hi)); }

    void read(const QSettings &settings)
    {
        if (m_key.empty()) {
            return;
        }
        const QVariant var = settings.value(QString::fromStdString(m_key),
                                            QVariant::fromValue(m_defaultValue));
        T val;
        if constexpr (std::is_same_v<T, Color>) {
            if (var.canConvert<Color>()) {
                val = var.value<Color>();
            } else if (var.canConvert<QColor>()) {
                val = Color(var.value<QColor>());
            } else if (var.canConvert<QString>()) {
                val = Color::fromHex(var.toString().toStdString());
            } else {
                val = m_defaultValue;
            }
        } else {
            if (var.canConvert<T>()) {
                val = var.value<T>();
            } else {
                val = m_defaultValue;
            }
        }

        if (m_validator) {
            val = m_validator(val);
        }
        set(val);
    }

    void write(QSettings &settings) const
    {
        if (m_key.empty()) {
            return;
        }
        settings.setValue(QString::fromStdString(m_key), QVariant::fromValue(m_value));
    }

    void registerChangeCallback(const ChangeMonitor::Lifetime &lifetime,
                                const ChangeMonitor::Function &callback) const
    {
        m_changeMonitor.registerChangeCallback(lifetime, callback);
    }
};
