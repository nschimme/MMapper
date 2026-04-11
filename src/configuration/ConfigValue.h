#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "../global/ChangeMonitor.h"
#include "../global/Color.h"
#include "../global/RAII.h"
#include "../global/RuleOf5.h"
#include "../global/TextUtils.h"
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
        , m_changeMonitor{}
        , m_notifying{false}
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

    NODISCARD bool operator==(const ConfigValue &rhs) const { return m_value == rhs.m_value; }
    NODISCARD bool operator!=(const ConfigValue &rhs) const { return !operator==(rhs); }

    NODISCARD bool operator==(const T &rhs) const { return m_value == rhs; }
    NODISCARD bool operator!=(const T &rhs) const { return !operator==(rhs); }

public:
    NODISCARD const T &get() const { return m_value; }
    NODISCARD operator T() const { return m_value; }
    NODISCARD const T *operator->() const { return &m_value; }

    NODISCARD const std::string &getKey() const { return m_key; }
    NODISCARD const std::string &getLabel() const { return m_label; }
    NODISCARD const T &getDefaultValue() const { return m_defaultValue; }

    void set(const T &newValue)
    {
        if (m_notifying) {
            return;
        }

        T validatedValue = newValue;
        if (m_validator) {
            validatedValue = m_validator(newValue);
        }

        if (utils::equals(m_value, validatedValue)) {
            return;
        }
        m_value = validatedValue;
        notifyChanged();
    }

    void notifyChanged()
    {
        m_notifying = true;
        const auto notify_exit = RAIICallback([this]() { m_notifying = false; });
        m_changeMonitor.notifyAll();
    }

    void reset() { set(m_defaultValue); }

    void clamp(const T &lo, const T &hi) { set(std::clamp(m_value, lo, hi)); }

    void read(const QSettings &settings)
    {
        if (m_key.empty()) {
            return;
        }
        const QVariant var = settings.value(mmqt::toQStringUtf8(m_key));
        if (!var.isValid()) {
            return;
        }

        if constexpr (std::is_same_v<T, Color>) {
            if (var.canConvert<Color>()) {
                set(var.value<Color>());
            } else if (var.canConvert<QColor>()) {
                set(Color(var.value<QColor>()));
            } else if (var.canConvert<QString>()) {
                QString s = var.toString();
                if (s.startsWith(u'#')) {
                    s.remove(0, 1);
                }
                set(Color::fromHex(mmqt::toStdStringUtf8(s)));
            }
        } else if constexpr (std::is_same_v<T, QColor>) {
            if (var.canConvert<QColor>()) {
                set(var.value<QColor>());
            } else if (var.canConvert<Color>()) {
                set(var.value<Color>().getQColor());
            } else if (var.canConvert<QString>()) {
                set(QColor(var.toString()));
            }
        } else if constexpr (std::is_enum_v<T>) {
            if (var.canConvert<T>()) {
                set(var.value<T>());
            } else if (var.canConvert<int>()) {
                set(static_cast<T>(var.toInt()));
            }
        } else {
            if (var.canConvert<T>()) {
                set(var.value<T>());
            }
        }
    }

    void write(QSettings &settings) const
    {
        if (m_key.empty()) {
            return;
        }
        settings.setValue(mmqt::toQStringUtf8(m_key), QVariant::fromValue(m_value));
    }

    void registerChangeCallback(const ChangeMonitor::Lifetime &lifetime,
                                const ChangeMonitor::Function &callback) const
    {
        m_changeMonitor.registerChangeCallback(lifetime, callback);
    }
};
