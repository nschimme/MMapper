// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "HotkeyManager.h"

#include "../global/TextUtils.h"
#include "HotkeyMacros.h"

#include <unordered_map>
#include <unordered_set>

#include <QDebug>
#include <QRegularExpression>
#include <QSettings>
#include <QTextStream>

namespace {
constexpr const char *SETTINGS_RAW_CONTENT_KEY = "IntegratedClient/HotkeysRawContent";

// Default hotkeys content preserving order and formatting
const QString DEFAULT_HOTKEYS_CONTENT = R"(# Hotkey Configuration
# Format: _hotkey KEY command
# Lines starting with # are comments.

# Basic movement (numpad)
_hotkey NUMPAD8 n
_hotkey NUMPAD4 w
_hotkey NUMPAD6 e
_hotkey NUMPAD5 s
_hotkey NUMPAD_MINUS u
_hotkey NUMPAD_PLUS d

# Open exit (CTRL+numpad)
_hotkey CTRL+NUMPAD8 open exit n
_hotkey CTRL+NUMPAD4 open exit w
_hotkey CTRL+NUMPAD6 open exit e
_hotkey CTRL+NUMPAD5 open exit s
_hotkey CTRL+NUMPAD_MINUS open exit u
_hotkey CTRL+NUMPAD_PLUS open exit d

# Close exit (ALT+numpad)
_hotkey ALT+NUMPAD8 close exit n
_hotkey ALT+NUMPAD4 close exit w
_hotkey ALT+NUMPAD6 close exit e
_hotkey ALT+NUMPAD5 close exit s
_hotkey ALT+NUMPAD_MINUS close exit u
_hotkey ALT+NUMPAD_PLUS close exit d

# Pick exit (SHIFT+numpad)
_hotkey SHIFT+NUMPAD8 pick exit n
_hotkey SHIFT+NUMPAD4 pick exit w
_hotkey SHIFT+NUMPAD6 pick exit e
_hotkey SHIFT+NUMPAD5 pick exit s
_hotkey SHIFT+NUMPAD_MINUS pick exit u
_hotkey SHIFT+NUMPAD_PLUS pick exit d

# Other actions
_hotkey NUMPAD7 look
_hotkey NUMPAD9 flee
_hotkey NUMPAD2 lead
_hotkey NUMPAD0 bash
_hotkey NUMPAD1 ride
_hotkey NUMPAD3 stand
)";

// Key name to Qt::Key mapping
const QHash<QString, int> &getKeyNameToQtKeyMap()
{
    static const QHash<QString, int> map = []() {
        QHash<QString, int> m;
#define ADD_TO_MAP(name, qtKey, isNumpad) m[name] = qtKey;
        X_FOREACH_HOTKEY(ADD_TO_MAP)
#undef ADD_TO_MAP
        return m;
    }();
    return map;
}

// Qt::Key to key name mapping (for non-numpad keys)
const QHash<int, QString> &getQtKeyToKeyNameMap()
{
    static const QHash<int, QString> map = []() {
        QHash<int, QString> m;
#define ADD_TO_MAP(name, qtKey, isNumpad) \
    if (!isNumpad) { \
        m[qtKey] = name; \
    }
        X_FOREACH_HOTKEY(ADD_TO_MAP)
#undef ADD_TO_MAP
        return m;
    }();
    return map;
}

// Numpad Qt::Key to key name mapping (requires KeypadModifier to be set)
const QHash<int, QString> &getNumpadQtKeyToKeyNameMap()
{
    static const QHash<int, QString> map = []() {
        QHash<int, QString> m;
#define ADD_TO_MAP(name, qtKey, isNumpad) \
    if (isNumpad) { \
        m[qtKey] = name; \
    }
        X_FOREACH_HOTKEY(ADD_TO_MAP)
#undef ADD_TO_MAP
        return m;
    }();
    return map;
}

// Static set of valid base key names for validation
const QSet<QString> &getValidBaseKeys()
{
    static const QSet<QString> validKeys = []() {
        QSet<QString> keys;
        for (const QString &key : HotkeyManager::getAvailableKeyNames()) {
            keys.insert(key);
        }
        return keys;
    }();
    return validKeys;
}

// Check if key name is a numpad key
bool isNumpadKeyName(const QString &keyName)
{
    return keyName.startsWith("NUMPAD");
}

} // namespace

HotkeyManager::HotkeyManager() = default;

void HotkeyManager::loadFromSettings(QSettings &settings)
{
    m_hotkeys.clear();
    m_orderedHotkeys.clear();

    m_rawContent = settings.value(SETTINGS_RAW_CONTENT_KEY).toString();

    if (m_rawContent.isEmpty()) {
        m_rawContent = DEFAULT_HOTKEYS_CONTENT;
    }

    parseRawContent();
}

void HotkeyManager::parseRawContent()
{
    static const QRegularExpression hotkeyRegex(R"(^\s*_hotkey\s+(\S+)\s+(.+)$)");

    m_hotkeys.clear();
    m_orderedHotkeys.clear();

    const QStringList lines = m_rawContent.split('\n');

    for (const QString &line : lines) {
        QString trimmedLine = line.trimmed();

        if (trimmedLine.isEmpty() || trimmedLine.startsWith('#')) {
            continue;
        }

        QRegularExpressionMatch match = hotkeyRegex.match(trimmedLine);
        if (match.hasMatch()) {
            QString keyStr = normalizeKeyString(match.captured(1));
            QString commandQStr = match.captured(2).trimmed();
            if (!keyStr.isEmpty() && !commandQStr.isEmpty()) {
                HotkeyKey hk = stringToHotkeyKey(keyStr);
                if (hk.key != 0) {
                    std::string command = mmqt::toStdStringUtf8(commandQStr);
                    m_hotkeys[hk] = command;
                    m_orderedHotkeys.emplace_back(keyStr, command);
                }
            }
        }
    }
}

void HotkeyManager::saveToSettings(QSettings &settings) const
{
    settings.setValue(SETTINGS_RAW_CONTENT_KEY, m_rawContent);
}

bool HotkeyManager::setHotkey(const QString &keyName, const QString &command)
{
    QString normalizedKey = normalizeKeyString(keyName);
    if (normalizedKey.isEmpty()) {
        return false;
    }

    static const QRegularExpression hotkeyLineRegex(R"(^(\s*_hotkey\s+)(\S+)(\s+)(.+)$)",
                                                    QRegularExpression::MultilineOption);

    QString newLine = "_hotkey " + normalizedKey + " " + command;
    bool found = false;

    QStringList lines = m_rawContent.split('\n');
    for (int i = 0; i < lines.size(); ++i) {
        QRegularExpressionMatch match = hotkeyLineRegex.match(lines[i]);
        if (match.hasMatch()) {
            QString existingKey = normalizeKeyString(match.captured(2));
            if (existingKey == normalizedKey) {
                lines[i] = newLine;
                found = true;
                break;
            }
        }
    }

    if (!found) {
        if (!m_rawContent.endsWith('\n')) {
            m_rawContent += '\n';
        }
        m_rawContent += newLine + '\n';
    } else {
        m_rawContent = lines.join('\n');
    }

    parseRawContent();
    return true;
}

void HotkeyManager::removeHotkey(const QString &keyName)
{
    QString normalizedKey = normalizeKeyString(keyName);
    if (normalizedKey.isEmpty()) {
        return;
    }

    HotkeyKey hk = stringToHotkeyKey(normalizedKey);
    if (m_hotkeys.count(hk) == 0) {
        return;
    }

    static const QRegularExpression hotkeyLineRegex(R"(^\s*_hotkey\s+(\S+)\s+.+$)");

    QStringList lines = m_rawContent.split('\n');
    QStringList newLines;

    for (const QString &line : lines) {
        QRegularExpressionMatch match = hotkeyLineRegex.match(line);
        if (match.hasMatch()) {
            QString existingKey = normalizeKeyString(match.captured(1));
            if (existingKey == normalizedKey) {
                continue;
            }
        }
        newLines.append(line);
    }

    m_rawContent = newLines.join('\n');

    parseRawContent();
}

std::string HotkeyManager::getCommand(int key, Qt::KeyboardModifiers modifiers, bool isNumpad) const
{
    HotkeyKey hk(key, modifiers & ~Qt::KeypadModifier, isNumpad);
    auto it = m_hotkeys.find(hk);
    if (it != m_hotkeys.end()) {
        return it->second;
    }
    return std::string();
}

std::string HotkeyManager::getCommand(const QString &keyName) const
{
    QString normalizedKey = normalizeKeyString(keyName);
    if (normalizedKey.isEmpty()) {
        return std::string();
    }

    HotkeyKey hk = stringToHotkeyKey(normalizedKey);
    if (hk.key == 0) {
        return std::string();
    }

    auto it = m_hotkeys.find(hk);
    if (it != m_hotkeys.end()) {
        return it->second;
    }
    return std::string();
}

QString HotkeyManager::getCommandQString(int key,
                                         Qt::KeyboardModifiers modifiers,
                                         bool isNumpad) const
{
    const std::string cmd = getCommand(key, modifiers, isNumpad);
    if (cmd.empty()) {
        return QString();
    }
    return mmqt::toQStringUtf8(cmd);
}

QString HotkeyManager::getCommandQString(const QString &keyName) const
{
    const std::string cmd = getCommand(keyName);
    if (cmd.empty()) {
        return QString();
    }
    return mmqt::toQStringUtf8(cmd);
}

bool HotkeyManager::hasHotkey(const QString &keyName) const
{
    QString normalizedKey = normalizeKeyString(keyName);
    if (normalizedKey.isEmpty()) {
        return false;
    }

    HotkeyKey hk = stringToHotkeyKey(normalizedKey);
    return hk.key != 0 && m_hotkeys.count(hk) > 0;
}

QString HotkeyManager::normalizeKeyString(const QString &keyString)
{
    QStringList parts = keyString.split('+', Qt::SkipEmptyParts);

    if (parts.isEmpty()) {
        qWarning() << "HotkeyManager: empty or invalid key string:" << keyString;
        return QString();
    }

    QString baseKey = parts.last();
    parts.removeLast();

    QStringList normalizedParts;

    bool hasCtrl = false;
    bool hasShift = false;
    bool hasAlt = false;
    bool hasMeta = false;

    for (const QString &part : parts) {
        QString upperPart = part.toUpper().trimmed();
        if (upperPart == "CTRL" || upperPart == "CONTROL") {
            hasCtrl = true;
        } else if (upperPart == "SHIFT") {
            hasShift = true;
        } else if (upperPart == "ALT") {
            hasAlt = true;
        } else if (upperPart == "META" || upperPart == "CMD" || upperPart == "COMMAND") {
            hasMeta = true;
        } else {
            qWarning() << "HotkeyManager: unrecognized modifier:" << part << "in:" << keyString;
        }
    }

    QString upperBaseKey = baseKey.toUpper();
    if (!isValidBaseKey(upperBaseKey)) {
        qWarning() << "HotkeyManager: invalid base key:" << baseKey << "in:" << keyString;
        return QString();
    }

    if (hasCtrl) {
        normalizedParts << "CTRL";
    }
    if (hasShift) {
        normalizedParts << "SHIFT";
    }
    if (hasAlt) {
        normalizedParts << "ALT";
    }
    if (hasMeta) {
        normalizedParts << "META";
    }

    normalizedParts << upperBaseKey;

    return normalizedParts.join("+");
}

HotkeyKey HotkeyManager::stringToHotkeyKey(const QString &keyString)
{
    QString normalized = normalizeKeyString(keyString);
    if (normalized.isEmpty()) {
        return HotkeyKey();
    }

    QStringList parts = normalized.split('+', Qt::SkipEmptyParts);
    if (parts.isEmpty()) {
        return HotkeyKey();
    }

    QString baseKey = parts.last();
    parts.removeLast();

    Qt::KeyboardModifiers mods = Qt::NoModifier;
    for (const QString &part : parts) {
        if (part == "CTRL") {
            mods |= Qt::ControlModifier;
        } else if (part == "SHIFT") {
            mods |= Qt::ShiftModifier;
        } else if (part == "ALT") {
            mods |= Qt::AltModifier;
        } else if (part == "META") {
            mods |= Qt::MetaModifier;
        }
    }

    bool isNumpad = isNumpadKeyName(baseKey);

    int qtKey = baseKeyNameToQtKey(baseKey);
    if (qtKey == 0) {
        return HotkeyKey();
    }

    return HotkeyKey(qtKey, mods, isNumpad);
}

QString HotkeyManager::hotkeyKeyToString(const HotkeyKey &hk)
{
    if (hk.key == 0) {
        return QString();
    }

    QStringList parts;

    if (hk.modifiers & Qt::ControlModifier) {
        parts << "CTRL";
    }
    if (hk.modifiers & Qt::ShiftModifier) {
        parts << "SHIFT";
    }
    if (hk.modifiers & Qt::AltModifier) {
        parts << "ALT";
    }
    if (hk.modifiers & Qt::MetaModifier) {
        parts << "META";
    }

    QString keyName;
    if (hk.isNumpad) {
        keyName = getNumpadQtKeyToKeyNameMap().value(hk.key);
    }
    if (keyName.isEmpty()) {
        keyName = qtKeyToBaseKeyName(hk.key);
    }
    if (keyName.isEmpty()) {
        return QString();
    }
    parts << keyName;

    return parts.join("+");
}

int HotkeyManager::baseKeyNameToQtKey(const QString &keyName)
{
    auto it = getKeyNameToQtKeyMap().find(keyName.toUpper());
    if (it != getKeyNameToQtKeyMap().end()) {
        return it.value();
    }
    return 0;
}

QString HotkeyManager::qtKeyToBaseKeyName(int qtKey)
{
    auto it = getQtKeyToKeyNameMap().find(qtKey);
    if (it != getQtKeyToKeyNameMap().end()) {
        return it.value();
    }

    return QString();
}

void HotkeyManager::resetToDefaults()
{
    m_rawContent = DEFAULT_HOTKEYS_CONTENT;
    parseRawContent();
}

void HotkeyManager::clear()
{
    m_hotkeys.clear();
    m_orderedHotkeys.clear();
    m_rawContent.clear();
}

std::vector<QString> HotkeyManager::getAllKeyNames() const
{
    std::vector<QString> result;
    result.reserve(m_orderedHotkeys.size());
    for (const auto &pair : m_orderedHotkeys) {
        result.push_back(pair.first);
    }
    return result;
}

QString HotkeyManager::exportToCliFormat() const
{
    return m_rawContent;
}

int HotkeyManager::importFromCliFormat(const QString &content)
{
    m_rawContent = content;
    parseRawContent();
    return static_cast<int>(m_orderedHotkeys.size());
}

bool HotkeyManager::isValidBaseKey(const QString &baseKey)
{
    return getValidBaseKeys().contains(baseKey.toUpper());
}

std::vector<QString> HotkeyManager::getAvailableKeyNames()
{
    std::vector<QString> names;
#define ADD_TO_VECTOR(name, qtKey, isNumpad) names.push_back(name);
    X_FOREACH_HOTKEY(ADD_TO_VECTOR)
#undef ADD_TO_VECTOR
    return names;
}

std::vector<QString> HotkeyManager::getAvailableModifiers()
{
    return std::vector<QString>{"CTRL", "SHIFT", "ALT", "META"};
}