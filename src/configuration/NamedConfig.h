#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "../global/ChangeMonitor.h"
#include "../global/Color.h"
#include "../global/INamedConfig.h"

#include <QString>
#include <QColor>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <functional>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

template<typename T>
class NODISCARD NamedConfig final : public INamedConfig
{
public:
    using OnAfterChange = std::function<void(const T &)>;

private:
    std::string m_name;
    ChangeMonitor m_changeMonitor;
    T m_value;
    OnAfterChange m_onAfterChange;
    bool m_notifying = false;

public:
    NamedConfig() = delete;
    DELETE_CTORS_AND_ASSIGN_OPS(NamedConfig);

    explicit NamedConfig(std::string name, T initialValue, OnAfterChange onAfterChange = nullptr)
        : m_name{std::move(name)}
        , m_value{std::move(initialValue)}
        , m_onAfterChange{std::move(onAfterChange)}
    {}

    explicit NamedConfig(std::string name, T initialValue, std::function<void()> simpleCallback)
        : m_name{std::move(name)}
        , m_value{std::move(initialValue)}
    {
        if (simpleCallback) {
            m_onAfterChange = [cb = std::move(simpleCallback)](const T &) { cb(); };
        }
    }

public:
    NODISCARD const std::string &getName() const override { return m_name; }
    NODISCARD inline T get() const { return m_value; }
    void set(const T newValue)
    {
        if (m_notifying) {
            throw std::runtime_error("recursion");
        }

        if (m_value == newValue) {
            return;
        }

        struct NODISCARD NotificationGuard final
        {
            NamedConfig &m_self;
            NotificationGuard() = delete;
            DELETE_CTORS_AND_ASSIGN_OPS(NotificationGuard);
            explicit NotificationGuard(NamedConfig &self)
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
        if (m_onAfterChange) {
            m_onAfterChange(m_value);
        }
        m_changeMonitor.notifyAll();
    }

    void setFromNotifier(std::function<void()> simpleCallback)
    {
        if (simpleCallback) {
            m_onAfterChange = [cb = std::move(simpleCallback)](const T &) { cb(); };
        } else {
            m_onAfterChange = nullptr;
        }
    }

    void clamp(const T lo, const T hi)
    {
        // don't try to call this for boolean or string.
        static_assert(std::is_integral_v<T> || std::is_floating_point_v<T>);
        if constexpr (std::is_floating_point_v<T>) {
            assert(std::isfinite(lo));
            assert(std::isfinite(hi));
        }
        assert(lo <= hi);
        set(std::clamp(get(), lo, hi));
    }

public:
    std::string toString() const override
    {
        if constexpr (std::is_same_v<T, std::string>) {
            return m_value;
        } else if constexpr (std::is_same_v<T, QString>) {
            return m_value.toStdString();
        } else if constexpr (std::is_same_v<T, bool>) {
            return m_value ? "true" : "false";
        } else if constexpr (std::is_integral_v<T> || std::is_floating_point_v<T>) {
            return std::to_string(m_value);
        } else if constexpr (std::is_same_v<T, Color>) {
            return m_value.toHex();
        } else if constexpr (std::is_same_v<T, QColor>) {
            return m_value.name().toStdString();
        } else if constexpr (std::is_same_v<T, QByteArray>) {
            return m_value.toBase64().toStdString();
        } else {
            return "";
        }
    }

    bool fromString(const std::string &str) override
    {
        if constexpr (std::is_same_v<T, std::string>) {
            set(str);
            return true;
        } else if constexpr (std::is_same_v<T, QString>) {
            set(QString::fromStdString(str));
            return true;
        } else if constexpr (std::is_same_v<T, bool>) {
            if (str == "true" || str == "1" || str == "on") {
                set(true);
                return true;
            } else if (str == "false" || str == "0" || str == "off") {
                set(false);
                return true;
            }
            return false;
        } else if constexpr (std::is_integral_v<T>) {
            try {
                set(static_cast<T>(std::stoll(str)));
                return true;
            } catch (...) {
                return false;
            }
        } else if constexpr (std::is_floating_point_v<T>) {
            try {
                set(static_cast<T>(std::stold(str)));
                return true;
            } catch (...) {
                return false;
            }
        } else if constexpr (std::is_same_v<T, Color>) {
            try {
                set(Color::fromHex(str));
                return true;
            } catch (...) {
                return false;
            }
        } else if constexpr (std::is_same_v<T, QColor>) {
            QColor c(QString::fromStdString(str));
            if (c.isValid()) {
                set(c);
                return true;
            }
            return false;
        } else if constexpr (std::is_same_v<T, QByteArray>) {
            set(QByteArray::fromBase64(QByteArray::fromStdString(str)));
            return true;
        } else {
            return false;
        }
    }

    void registerChangeCallback(const ChangeMonitor::Lifetime &lifetime,
                                ChangeMonitor::Function callback) override
    {
        return m_changeMonitor.registerChangeCallback(lifetime, std::move(callback));
    }
};
