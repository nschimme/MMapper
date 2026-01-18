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
    auto &group = setConfig().hotkeys;
    group.registerCallbacks([this](const QSettings &s) { this->read(s); },
                            [this](QSettings &s) { this->write(s); });

    // Initial read
    QSettings settings;
    settings.beginGroup(group.getName());
    read(settings);
    settings.endGroup();
}

void HotkeyManager::read(const QSettings &settings)
{
    clear();
    const QStringList keys = settings.childKeys();
    if (keys.isEmpty()) {
        resetToDefaults();
        return;
    }

    for (const QString &key : keys) {
        HotkeyCommand hk = HotkeyCommand::deserialize(key);
        if (hk.isValid()) {
            setHotkey(hk, mmqt::toStdStringUtf8(settings.value(key).toString()));
        }
    }
}

void HotkeyManager::write(QSettings &settings)
{
    settings.clear();
    for (const auto &hk : getAllHotkeys()) {
        settings.setValue(hk.first, mmqt::toQStringUtf8(hk.second));
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

    m_lookupTable[calculateIndex(hk.baseKey, hk.modifiers)] = command;
    setConfig().hotkeys.notifyChanged();
    return true;
}

void HotkeyManager::removeHotkey(const QString &keyName)
{
    HotkeyCommand hk = HotkeyCommand::deserialize(keyName);
    if (hk.isValid()) {
        m_lookupTable[calculateIndex(hk.baseKey, hk.modifiers)].clear();
        setConfig().hotkeys.notifyChanged();
    }
}

std::string HotkeyManager::getCommand(int key, Qt::KeyboardModifiers modifiers, bool isNumpad) const
{
    HotkeyKeyEnum baseKey = qtKeyToBaseKeyEnum(key, isNumpad);
    if (baseKey == HotkeyKeyEnum::INVALID)
        return std::string();

    uint8_t mask = HotkeyCommand::qtModifiersToMask(modifiers);
    return m_lookupTable[calculateIndex(baseKey, mask)];
}

std::string HotkeyManager::getCommand(const QString &keyName) const
{
    HotkeyCommand hk = HotkeyCommand::deserialize(keyName);
    if (!hk.isValid())
        return std::string();

    return m_lookupTable[calculateIndex(hk.baseKey, hk.modifiers)];
}

QString HotkeyManager::getCommandQString(int key,
                                         Qt::KeyboardModifiers modifiers,
                                         bool isNumpad) const
{
    return mmqt::toQStringUtf8(getCommand(key, modifiers, isNumpad));
}

QString HotkeyManager::getCommandQString(const QString &keyName) const
{
    return mmqt::toQStringUtf8(getCommand(keyName));
}

bool HotkeyManager::hasHotkey(const QString &keyName) const
{
    return !getCommand(keyName).empty();
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
    clear();
#define X_DEFAULT(key, cmd) setHotkey(key, cmd);
    XFOREACH_DEFAULT_HOTKEYS(X_DEFAULT)
#undef X_DEFAULT
}

void HotkeyManager::clear()
{
    m_lookupTable.fill(std::string());
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
