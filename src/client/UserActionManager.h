#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "../global/ChangeMonitor.h"
#include "../global/RuleOf5.h"
#include "../global/StringView.h"
#include "../global/macros.h"

#include <memory>
#include <optional>
#include <regex>
#include <string>
#include <unordered_map>
#include <vector>

#include <QVariantMap>

enum class UserActionType { Regex, StartsWith, EndsWith };

struct UserAction
{
    UserActionType type;
    std::string pattern;
    std::string command;

    NODISCARD std::string serialize() const;
    NODISCARD static std::unique_ptr<UserAction> deserialize(const std::string &pattern,
                                                             const std::string &serialized);
};

struct CompiledUserAction
{
    UserAction action;
    std::optional<std::regex> regex;
    char hint = 0;

    explicit CompiledUserAction(UserAction a);
};

class UserActionManager
{
private:
    std::unordered_map<std::string, std::shared_ptr<CompiledUserAction>> m_actions;
    std::unordered_multimap<char, std::shared_ptr<CompiledUserAction>> m_actionMap;
    ChangeMonitor::Lifetime m_configLifetime;

public:
    UserActionManager();
    ~UserActionManager() = default;
    DELETE_CTORS_AND_ASSIGN_OPS(UserActionManager);

    void syncFromConfig();
    NODISCARD bool setAction(UserActionType type, std::string pattern, std::string command);
    NODISCARD bool removeAction(const std::string &pattern);
    NODISCARD std::vector<const UserAction *> getAllActions() const;

    void evaluate(StringView line,
                  const std::function<void(const std::string &)> &executeCallback) const;

private:
    static std::string substitute(const std::string &command,
                                  const std::vector<std::string> &captures);
};
