// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "HotkeyManager.h"

#include "../configuration/configuration.h"
#include "../global/TextUtils.h"

#include <QDebug>
#include <QSettings>

uint8_t HotkeyCommand::qtModifiersToMask(Qt::KeyboardModifiers mods)
{
    uint8_t mask = 0;
    if (mods & Qt::ShiftModifier)
        mask |= 1;
    if (mods & Qt::ControlModifier)
        mask |= 2;
    if (mods & Qt::AltModifier)
        mask |= 4;
    if (mods & Qt::MetaModifier)
        mask |= 8;
    return mask;
}

QString HotkeyCommand::serialize() const
{
    if (!isValid())
        return QString();

    QStringList parts;
    if (modifiers & 2)
        parts << "CTRL";
    if (modifiers & 1)
        parts << "SHIFT";
    if (modifiers & 4)
        parts << "ALT";
    if (modifiers & 8)
        parts << "META";

    QString keyName;
#define X_NAME(id, name, key, numpad) \
    case HotkeyKeyEnum::id: \
        keyName = name; \
        break;
    switch (baseKey) {
        XFOREACH_HOTKEY_BASE_KEYS(X_NAME)
    default:
        return QString();
    }
#undef X_NAME

    parts << keyName;
    return parts.join("+");
}

HotkeyCommand HotkeyCommand::deserialize(const QString &s)
{
    QStringList parts = s.toUpper().split('+', Qt::SkipEmptyParts);
    if (parts.isEmpty())
        return HotkeyCommand();

    uint8_t mods = 0;
    HotkeyKeyEnum baseKey = HotkeyKeyEnum::INVALID;

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
            // Assume it's the base key
            baseKey = HotkeyManager::getAvailableKeyNames().size() > 0
                          ? [] (const QString& name) {
#define X_CHECK(id, str, key, numpad) if (name == str) return HotkeyKeyEnum::id;
                              XFOREACH_HOTKEY_BASE_KEYS(X_CHECK)
#undef X_CHECK
                              return HotkeyKeyEnum::INVALID;
                          }(part)
                          : HotkeyKeyEnum::INVALID;
        }
    }

    return HotkeyCommand(baseKey, mods);
}

HotkeyManager::HotkeyManager()
{
    setConfig().hotkeys.registerChangeCallback(m_configLifetime, [this]() { this->syncFromConfig(); });
    syncFromConfig();

    if (getAllHotkeys().empty()) {
        resetToDefaults();
    }
}

HotkeyManager::~HotkeyManager()
{
    // registerChangeCallback with Lifetime automatically unregisters
}

void HotkeyManager::syncFromConfig()
{
    m_lookupTable.fill(std::string());
    const QVariantMap &data = setConfig().hotkeys.data();
    for (auto it = data.begin(); it != data.end(); ++it) {
        HotkeyCommand hk = HotkeyCommand::deserialize(it.key());
        if (hk.isValid()) {
            m_lookupTable[calculateIndex(hk.baseKey, hk.modifiers)] = mmqt::toStdStringUtf8(it.value().toString());
        }
    }
}

bool HotkeyManager::setHotkey(const QString &keyName, const QString &command)
{
    HotkeyCommand hk = HotkeyCommand::deserialize(keyName);
    if (!hk.isValid())
        return false;

    return setHotkey(hk, mmqt::toStdStringUtf8(command));
}

bool HotkeyManager::setHotkey(const HotkeyCommand &hk, const std::string &command)
{
    if (!hk.isValid())
        return false;

    QVariantMap data = setConfig().hotkeys.data();
    data[hk.serialize()] = mmqt::toQStringUtf8(command);
    setConfig().hotkeys.setData(std::move(data));
    return true;
}

void HotkeyManager::removeHotkey(const QString &keyName)
{
    HotkeyCommand hk = HotkeyCommand::deserialize(keyName);
    if (hk.isValid()) {
        QVariantMap data = setConfig().hotkeys.data();
        data.remove(hk.serialize());
        setConfig().hotkeys.setData(std::move(data));
    }
}

std::optional<std::string> HotkeyManager::getCommand(int key,
                                                     Qt::KeyboardModifiers modifiers,
                                                     bool isNumpad) const
{
    HotkeyKeyEnum baseKey = qtKeyToBaseKeyEnum(key, isNumpad);
    if (baseKey == HotkeyKeyEnum::INVALID)
        return std::nullopt;

    uint8_t mask = HotkeyCommand::qtModifiersToMask(modifiers);
    return getCommand(baseKey, mask);
}

std::optional<std::string> HotkeyManager::getCommand(HotkeyKeyEnum key, uint8_t mask) const
{
    if (key == HotkeyKeyEnum::INVALID)
        return std::nullopt;

    const std::string &command = m_lookupTable[calculateIndex(key, mask)];
    if (command.empty())
        return std::nullopt;

    return command;
}

std::optional<std::string> HotkeyManager::getCommand(const QString &keyName) const
{
    HotkeyCommand hk = HotkeyCommand::deserialize(keyName);
    if (!hk.isValid())
        return std::nullopt;

    return getCommand(hk.baseKey, hk.modifiers);
}

std::optional<QString> HotkeyManager::getCommandQString(int key,
                                                        Qt::KeyboardModifiers modifiers,
                                                        bool isNumpad) const
{
    auto command = getCommand(key, modifiers, isNumpad);
    return command ? std::optional<QString>(mmqt::toQStringUtf8(*command)) : std::nullopt;
}

std::optional<QString> HotkeyManager::getCommandQString(HotkeyKeyEnum key, uint8_t mask) const
{
    auto command = getCommand(key, mask);
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

std::vector<std::pair<QString, std::string>> HotkeyManager::getAllHotkeys() const
{
    std::vector<std::pair<QString, std::string>> result;
    for (uint8_t i = 0; i < static_cast<uint8_t>(HotkeyKeyEnum::COUNT); ++i) {
        for (uint8_t m = 0; m < 16; ++m) {
            const std::string &cmd = m_lookupTable[calculateIndex(static_cast<HotkeyKeyEnum>(i), m)];
            if (!cmd.empty()) {
                HotkeyCommand hk(static_cast<HotkeyKeyEnum>(i), m);
                result.emplace_back(hk.serialize(), cmd);
            }
        }
    }
    return result;
}

void HotkeyManager::resetToDefaults()
{
    QVariantMap data;
#define X_DEFAULT(key, cmd) data[key] = QString(cmd);
    XFOREACH_DEFAULT_HOTKEYS(X_DEFAULT)
#undef X_DEFAULT
    setConfig().hotkeys.setData(std::move(data));
}

void HotkeyManager::clear()
{
    setConfig().hotkeys.setData(QVariantMap());
}

std::vector<QString> HotkeyManager::getAvailableKeyNames()
{
    return {
#define X_STR(id, name, key, numpad) name,
        XFOREACH_HOTKEY_BASE_KEYS(X_STR)
#undef X_STR
    };
}

std::vector<QString> HotkeyManager::getAvailableModifiers()
{
    return {"CTRL", "SHIFT", "ALT", "META"};
}

HotkeyKeyEnum HotkeyManager::qtKeyToBaseKeyEnum(int key, bool isNumpad)
{
#define X_QT_CHECK(id, name, qkey, num) \
    if (key == qkey && isNumpad == num) \
        return HotkeyKeyEnum::id;
    XFOREACH_HOTKEY_BASE_KEYS(X_QT_CHECK)
#undef X_QT_CHECK
    return HotkeyKeyEnum::INVALID;
}

QString HotkeyManager::baseKeyEnumToName(HotkeyKeyEnum key)
{
#define X_NAME_CHECK(id, name, qkey, num) \
    if (key == HotkeyKeyEnum::id) \
        return name;
    XFOREACH_HOTKEY_BASE_KEYS(X_NAME_CHECK)
#undef X_NAME_CHECK
    return QString();
}

HotkeyKeyEnum HotkeyManager::nameToBaseKeyEnum(const QString &name)
{
#define X_NAME_TO_ENUM(id, str, qkey, num) \
    if (name.toUpper() == QString(str).toUpper()) \
        return HotkeyKeyEnum::id;
    XFOREACH_HOTKEY_BASE_KEYS(X_NAME_TO_ENUM)
#undef X_NAME_TO_ENUM
    return HotkeyKeyEnum::INVALID;
}
