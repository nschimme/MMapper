// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "UserActionManager.h"

#include "../configuration/configuration.h"
#include "../global/TextUtils.h"
#include "../global/logging.h"

#include <algorithm>
#include <regex>

std::string UserAction::serialize() const
{
    std::string typeStr;
    switch (type) {
    case UserActionType::Regex:
        typeStr = "regex";
        break;
    case UserActionType::StartsWith:
        typeStr = "starts";
        break;
    case UserActionType::EndsWith:
        typeStr = "ends";
        break;
    }
    return typeStr + ":" + command;
}

std::unique_ptr<UserAction> UserAction::deserialize(const std::string &pattern,
                                                    const std::string &serialized)
{
    size_t colon = serialized.find(':');
    if (colon == std::string::npos) {
        return nullptr;
    }

    std::string typeStr = serialized.substr(0, colon);
    std::string command = serialized.substr(colon + 1);

    UserActionType type;
    if (typeStr == "regex") {
        type = UserActionType::Regex;
    } else if (typeStr == "starts") {
        type = UserActionType::StartsWith;
    } else if (typeStr == "ends") {
        type = UserActionType::EndsWith;
    } else {
        return nullptr;
    }

    return std::make_unique<UserAction>(UserAction{type, pattern, command});
}

UserActionManager::UserActionManager()
{
    setConfig().actions.registerChangeCallback(m_configLifetime, [this]() { syncFromConfig(); });
    setConfig().actions.registerResetCallback(m_configLifetime, [this]() {
        m_actions.clear();
        setConfig().actions.setData(QVariantMap());
    });
    syncFromConfig();
}

void UserActionManager::syncFromConfig()
{
    m_actions.clear();
    const QVariantMap &data = getConfig().actions.data();
    for (auto it = data.begin(); it != data.end(); ++it) {
        std::string pattern = it.key().toStdString();
        std::string serialized = it.value().toString().toStdString();
        if (auto action = UserAction::deserialize(pattern, serialized)) {
            m_actions[pattern] = std::move(action);
        } else {
            MMLOG_WARNING() << "invalid action" << it.key().toStdString()
                            << it.value().toString().toStdString();
        }
    }
}

bool UserActionManager::setAction(UserActionType type, std::string pattern, std::string command)
{
    if (pattern.empty()) {
        return false;
    }

    QVariantMap data = getConfig().actions.data();
    UserAction action{type, pattern, command};
    data[QString::fromStdString(pattern)] = QString::fromStdString(action.serialize());
    setConfig().actions.setData(std::move(data));
    return true;
}

bool UserActionManager::removeAction(const std::string &pattern)
{
    QVariantMap data = getConfig().actions.data();
    QString key = QString::fromStdString(pattern);
    if (!data.contains(key)) {
        return false;
    }
    data.remove(key);
    setConfig().actions.setData(std::move(data));
    return true;
}

std::vector<const UserAction *> UserActionManager::getAllActions() const
{
    std::vector<const UserAction *> result;
    for (const auto &pair : m_actions) {
        result.push_back(pair.second.get());
    }
    return result;
}

std::string UserActionManager::substitute(const std::string &command,
                                          const std::vector<std::string> &captures)
{
    std::string result;
    result.reserve(command.size());

    for (size_t i = 0; i < command.size(); ++i) {
        if (command[i] == '%' && i + 1 < command.size() && std::isdigit(command[i + 1])) {
            size_t idx = static_cast<size_t>(command[i + 1] - '0');
            if (idx < captures.size()) {
                result += captures[idx];
            }
            ++i;
        } else {
            result += command[i];
        }
    }
    return result;
}

void UserActionManager::evaluate(
    StringView line, const std::function<void(const std::string &)> &executeCallback) const
{
    for (const auto &pair : m_actions) {
        const UserAction &action = *pair.second;
        bool matched = false;
        std::vector<std::string> captures;

        switch (action.type) {
        case UserActionType::Regex: {
            try {
                std::regex re(action.pattern, std::regex::optimize);
                std::cmatch match;
                if (std::regex_search(line.begin(), line.end(), match, re)) {
                    matched = true;
                    captures.reserve(match.size());
                    for (size_t i = 0; i < match.size(); ++i) {
                        const auto &sub = match[i];
                        if (sub.matched) {
                            captures.emplace_back(sub.first, static_cast<size_t>(sub.length()));
                        } else {
                            captures.emplace_back();
                        }
                    }
                }
            } catch (const std::regex_error &) {
                // Ignore invalid regex in user actions
            }
            break;
        }
        case UserActionType::StartsWith:
            if (line.startsWith(std::string_view(action.pattern))) {
                matched = true;
                captures.emplace_back(line.toStdString());
            }
            break;
        case UserActionType::EndsWith:
            if (line.endsWith(std::string_view(action.pattern))) {
                matched = true;
                captures.emplace_back(line.toStdString());
            }
            break;
        }

        if (matched) {
            executeCallback(substitute(action.command, captures));
        }
    }
}
