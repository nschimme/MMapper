// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "HotkeyManager.h"

#include "../configuration/configuration.h"
#include "../global/TextUtils.h"

#include <QDebug>
#include <QSettings>

uint8_t Hotkey::qtModifiersToMask(Qt::KeyboardModifiers mods)
{
    uint8_t mask = 0;
    if (mods & Qt::ControlModifier)
        mask |= 1;
    if (mods & Qt::ShiftModifier)
        mask |= 2;
    if (mods & Qt::AltModifier)
        mask |= 4;
    if (mods & Qt::MetaModifier)
        mask |= 8;
    return mask;
}

Hotkey::Hotkey(HotkeyEnum base, uint8_t mods)
{
    if (base == HotkeyEnum::INVALID) {
        m_hotkey = HotkeyEnum::INVALID;
    } else {
        m_hotkey = static_cast<HotkeyEnum>(static_cast<uint16_t>(base) + (mods & 0xF));
    }
}

HotkeyEnum Hotkey::base() const
{
    if (!isValid())
        return HotkeyEnum::INVALID;
    return static_cast<HotkeyEnum>(static_cast<uint16_t>(m_hotkey) & 0xFFF0);
}

uint8_t Hotkey::modifiers() const
{
    if (!isValid())
        return 0;
    return static_cast<uint8_t>(static_cast<uint16_t>(m_hotkey) & 0xF);
}

QString Hotkey::serialize() const
{
    if (!isValid())
        return QString();

    QStringList parts;
    uint8_t mods = modifiers();
    if (mods & 1)
        parts << "CTRL";
    if (mods & 2)
        parts << "SHIFT";
    if (mods & 4)
        parts << "ALT";
    if (mods & 8)
        parts << "META";

    QString name = hotkeyBaseToName(base());
    if (name.isEmpty())
        return QString();

    parts << name;
    return parts.join("+");
}

Hotkey Hotkey::deserialize(const QString &s)
{
    QStringList parts = s.toUpper().split('+', Qt::SkipEmptyParts);
    if (parts.isEmpty())
        return Hotkey();

    uint8_t mods = 0;
    HotkeyEnum base = HotkeyEnum::INVALID;

    for (int i = 0; i < parts.size(); ++i) {
        QString part = parts[i].trimmed();
        if (part == "CTRL" || part == "CONTROL") {
            mods |= 2;
        } else if (part == "SHIFT") {
            mods |= 1;
        } else if (part == "ALT") {
            mods |= 4;
        } else if (part == "META" || part == "CMD" || part == "COMMAND") {
            mods |= 8;
        } else {
            base = nameToHotkeyBase(part);
        }
    }

    if (base == HotkeyEnum::INVALID)
        return Hotkey();

    return Hotkey(base, mods);
}

HotkeyEnum Hotkey::qtKeyToHotkeyBase(int key, bool isNumpad)
{
#define X_QT_CHECK(id, name, qkey, num) \
    if (key == qkey && isNumpad == num) \
        return HotkeyEnum::id;
    XFOREACH_HOTKEY_BASE_KEYS(X_QT_CHECK)
#undef X_QT_CHECK
    return HotkeyEnum::INVALID;
}

QString Hotkey::hotkeyBaseToName(HotkeyEnum base)
{
#define X_NAME_CHECK(id, name, qkey, num) \
    if (base == HotkeyEnum::id) \
        return name;
    XFOREACH_HOTKEY_BASE_KEYS(X_NAME_CHECK)
#undef X_NAME_CHECK
    return QString();
}

HotkeyEnum Hotkey::nameToHotkeyBase(const QString &name)
{
#define X_NAME_TO_ENUM(id, str, qkey, num) \
    if (name.toUpper() == QString(str).toUpper()) \
        return HotkeyEnum::id;
    XFOREACH_HOTKEY_BASE_KEYS(X_NAME_TO_ENUM)
#undef X_NAME_TO_ENUM
    return HotkeyEnum::INVALID;
}

std::vector<QString> Hotkey::getAvailableKeyNames()
{
    return {
#define X_STR(id, name, key, numpad) name,
        XFOREACH_HOTKEY_BASE_KEYS(X_STR)
#undef X_STR
    };
}

std::vector<QString> Hotkey::getAvailableModifiers()
{
    return {
#define X_STR(name) #name,
        XFOREACH_HOTKEY_MODIFIER(X_STR)
#undef X_STR
    };
}

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
        Hotkey hk = Hotkey::deserialize(it.key());
        if (hk.isValid()) {
            m_hotkeys[hk.toEnum()] = mmqt::toStdStringUtf8(it.value().toString());
        }
    }
}

bool HotkeyManager::setHotkey(const QString &keyName, const QString &command)
{
    Hotkey hk = Hotkey::deserialize(keyName);
    if (!hk.isValid())
        return false;

    return setHotkey(hk, mmqt::toStdStringUtf8(command));
}

bool HotkeyManager::setHotkey(const Hotkey &hk, const std::string &command)
{
    if (!hk.isValid())
        return false;

    QVariantMap data = getConfig().hotkeys.data();
    data[hk.serialize()] = mmqt::toQStringUtf8(command);
    setConfig().hotkeys.setData(std::move(data));
    return true;
}

void HotkeyManager::removeHotkey(const QString &keyName)
{
    Hotkey hk = Hotkey::deserialize(keyName);
    removeHotkey(hk);
}

void HotkeyManager::removeHotkey(const Hotkey &hk)
{
    if (hk.isValid()) {
        QVariantMap data = getConfig().hotkeys.data();
        data.remove(hk.serialize());
        setConfig().hotkeys.setData(std::move(data));
    }
}

std::optional<std::string> HotkeyManager::getCommand(int key,
                                                     Qt::KeyboardModifiers modifiers,
                                                     bool isNumpad) const
{
    HotkeyEnum base = Hotkey::qtKeyToHotkeyBase(key, isNumpad);
    if (base == HotkeyEnum::INVALID)
        return std::nullopt;

    uint8_t mask = Hotkey::qtModifiersToMask(modifiers);
    return getCommand(Hotkey(base, mask));
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

std::optional<std::string> HotkeyManager::getCommand(const QString &keyName) const
{
    Hotkey hk = Hotkey::deserialize(keyName);
    return getCommand(hk);
}

std::optional<QString> HotkeyManager::getCommandQString(int key,
                                                        Qt::KeyboardModifiers modifiers,
                                                        bool isNumpad) const
{
    auto command = getCommand(key, modifiers, isNumpad);
    return command ? std::optional<QString>(mmqt::toQStringUtf8(*command)) : std::nullopt;
}

std::optional<QString> HotkeyManager::getCommandQString(const Hotkey &hk) const
{
    auto command = getCommand(hk);
    return command ? std::optional<QString>(mmqt::toQStringUtf8(*command)) : std::nullopt;
}

std::optional<QString> HotkeyManager::getCommandQString(const QString &keyName) const
{
    auto command = getCommand(keyName);
    return command ? std::optional<QString>(mmqt::toQStringUtf8(*command)) : std::nullopt;
}

bool HotkeyManager::hasHotkey(const QString &keyName) const
{
    return getCommand(keyName).has_value();
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
