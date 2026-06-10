#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "../global/ChangeMonitor.h"
#include "../global/RuleOf5.h"
#include "../global/StringView.h"
#include "../global/macros.h"

#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <regex>
#include <string>
#include <unordered_map>
#include <vector>

#include <QObject>
#include <QString>
#include <QVariantMap>

enum class ScriptActionType { Auto, Regex, Wildcard, Starts, Ends };

struct ScriptAction
{
    ScriptActionType type;
    std::string pattern;
    std::string command;
};

struct CompiledScriptAction
{
    ScriptAction action;
    std::optional<std::regex> regex;
    char hint = 0;

    explicit CompiledScriptAction(ScriptAction a);
};

class ScriptEngine : public QObject
{
    Q_OBJECT

public:
    using ExecuteCallback = std::function<void(const std::string &)>;
    using LogCallback = std::function<void(const std::string &)>;

private:
    std::unordered_map<std::string, std::string> m_variables;
    std::unordered_map<std::string, std::string> m_aliases;
    std::map<std::string, std::shared_ptr<CompiledScriptAction>> m_actions;
    std::unordered_multimap<char, std::shared_ptr<CompiledScriptAction>> m_actionMap;

    std::vector<std::string> m_captures; // %0..%9 for the current action

    ChangeMonitor::Lifetime m_configLifetime;
    ExecuteCallback m_executeCallback;
    LogCallback m_logCallback;
    bool m_isSaving = false;

public:
    explicit ScriptEngine(QObject *parent = nullptr);
    ~ScriptEngine() override = default;
    DELETE_CTORS_AND_ASSIGN_OPS(ScriptEngine);

    void setExecuteCallback(ExecuteCallback callback) { m_executeCallback = callback; }
    void setLogCallback(LogCallback callback) { m_logCallback = callback; }

    void syncFromConfig();

    // Core commands
    void setVariable(const std::string &name, const std::string &value);
    void setAlias(const std::string &name, const std::string &command);
    void setAction(const std::string &pattern,
                   const std::string &command,
                   ScriptActionType type = ScriptActionType::Auto);

    bool removeVariable(const std::string &name);
    bool removeAlias(const std::string &name);
    bool removeAction(const std::string &pattern);

    NODISCARD std::vector<const ScriptAction *> getAllActions() const;
    NODISCARD std::map<std::string, std::string> getAllVariables() const;
    NODISCARD std::map<std::string, std::string> getAllAliases() const;

    // Evaluation
    void processServerFeed(StringView line);
    bool processUserInput(std::string &input); // Returns true if intercepted/modified

    // TinTin++ style parsing
    std::vector<std::string> parseArguments(StringView &input);
    std::string expandVariables(const std::string &input);
    void executeScript(const std::string &script);

    // Internal command handling
    bool executePrimitive(const std::string &fullCommand);

    // Expression evaluation for #IF
    bool evaluateExpression(const std::string &expr);

private:
    void saveVariables();
    void saveAliases();
    void saveActions();

public:
    static std::string wildcardToRegex(const std::string &wildcard);

private:
    std::string substitute(const std::string &command, const std::vector<std::string> &captures);

    void logScriptError(const std::string &error);
    void logScriptInfo(const std::string &info);
};
