// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "HotkeyManager.h"

#include "../configuration/configuration.h"
#include "../global/TextUtils.h"

#include <QDebug>
#include <QSettings>

HotkeyManager::HotkeyManager()
{
    setConfig().hotkeys.registerChangeCallback(m_configLifetime,
                                               [this]() { this->syncFromConfig(); });
    syncFromConfig();

    if (m_hotkeys.empty()) {
        resetToDefaults();
    }
}

HotkeyManager::~HotkeyManager() {}

void HotkeyManager::syncFromConfig()
{
    m_hotkeys.clear();
    const QVariantMap &data = getConfig().hotkeys.data();
    for (auto it = data.begin(); it != data.end(); ++it) {
        Hotkey hk(it.key());
        if (hk.isValid()) {
            m_hotkeys[hk.toEnum()] = mmqt::toStdStringUtf8(it.value().toString());
        }
    }
}

bool HotkeyManager::setHotkey(const Hotkey &hk, std::string command)
{
    if (!hk.isValid())
        return false;

    QVariantMap data = getConfig().hotkeys.data();
    data[mmqt::toQStringUtf8(hk.serialize())] = mmqt::toQStringUtf8(command);
    setConfig().hotkeys.setData(std::move(data));
    return true;
}

void HotkeyManager::removeHotkey(const Hotkey &hk)
{
    if (hk.isValid()) {
        QVariantMap data = getConfig().hotkeys.data();
        data.remove(mmqt::toQStringUtf8(hk.serialize()));
        setConfig().hotkeys.setData(std::move(data));
    }
}

std::optional<std::string> HotkeyManager::getCommand(const Hotkey &hk) const
{
    if (!hk.isValid())
        return std::nullopt;

    auto it = m_hotkeys.find(hk.toEnum());
    if (it == m_hotkeys.end())
        return std::nullopt;
    return it->second;
}

bool HotkeyManager::hasHotkey(const Hotkey &hk) const
{
    return getCommand(hk).has_value();
}

std::vector<std::pair<Hotkey, std::string>> HotkeyManager::getAllHotkeys() const
{
    std::vector<std::pair<Hotkey, std::string>> result;
    for (const auto &[key, cmd] : m_hotkeys) {
        result.emplace_back(Hotkey(key), cmd);
    }
    return result;
}

void HotkeyManager::resetToDefaults()
{
    m_hotkeys.clear();
    QVariantMap data;
#define X_DEFAULT(key, cmd) data[key] = QString(cmd);
    XFOREACH_DEFAULT_HOTKEYS(X_DEFAULT)
#undef X_DEFAULT
    setConfig().hotkeys.setData(std::move(data));
}

void HotkeyManager::clear()
{
    m_hotkeys.clear();
    setConfig().hotkeys.setData(QVariantMap());
}
