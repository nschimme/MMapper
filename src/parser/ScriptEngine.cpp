// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "ScriptEngine.h"

#include "../configuration/configuration.h"
#include "../global/TextUtils.h"
#include "../global/logging.h"

#include <algorithm>
#include <regex>
#include <sstream>

CompiledScriptAction::CompiledScriptAction(ScriptAction a)
    : action(std::move(a))
{
    using namespace char_consts;
    std::string regexPattern;
    if (action.type == ScriptActionType::Wildcard) {
        regexPattern = ScriptEngine::wildcardToRegex(action.pattern);
    } else {
        regexPattern = action.pattern;
    }

    try {
        regex.emplace(regexPattern, std::regex::optimize);
        if (!regexPattern.empty() && regexPattern[0] == C_CARET) {
            if (regexPattern.length() > 1 && regexPattern[1] != C_BACKSLASH
                && regexPattern[1] != C_OPEN_PARENS) {
                hint = regexPattern[1];
            }
        }
    } catch (const std::regex_error &e) {
        MMLOG_WARNING() << "Invalid regex in action:" << action.pattern << ":" << e.what();
    }
}

ScriptEngine::ScriptEngine(QObject *parent)
    : QObject(parent)
{
    auto sync = [this]() { syncFromConfig(); };
    setConfig().variables.registerChangeCallback(m_configLifetime, sync);
    setConfig().aliases.registerChangeCallback(m_configLifetime, sync);
    setConfig().actions.registerChangeCallback(m_configLifetime, sync);

    syncFromConfig();
}

void ScriptEngine::syncFromConfig()
{
    m_variables.clear();
    const QVariantMap &varData = getConfig().variables.data();
    for (auto it = varData.begin(); it != varData.end(); ++it) {
        m_variables[mmqt::toStdStringUtf8(it.key())] = mmqt::toStdStringUtf8(it.value().toString());
    }

    m_aliases.clear();
    const QVariantMap &aliasData = getConfig().aliases.data();
    for (auto it = aliasData.begin(); it != aliasData.end(); ++it) {
        m_aliases[mmqt::toStdStringUtf8(it.key())] = mmqt::toStdStringUtf8(it.value().toString());
    }

    m_actions.clear();
    m_actionMap.clear();
    const QVariantMap &actionData = getConfig().actions.data();
    for (auto it = actionData.begin(); it != actionData.end(); ++it) {
        std::string pattern = mmqt::toStdStringUtf8(it.key());
        std::string command = mmqt::toStdStringUtf8(it.value().toString());

        // Simple heuristic to distinguish wildcard vs regex
        ScriptActionType type = ScriptActionType::Wildcard;
        if (pattern.find_first_of("^$.()[]{}?+|") != std::string::npos) {
            type = ScriptActionType::Regex;
        }

        auto compiled = std::make_shared<CompiledScriptAction>(ScriptAction{type, pattern, command});
        m_actions[pattern] = compiled;
        m_actionMap.emplace(compiled->hint, compiled);
    }
}

void ScriptEngine::setVariable(const std::string &name, const std::string &value)
{
    if (name.empty()) {
        return;
    }
    m_variables[name] = value;
    logScriptInfo("[VARIABLE: " + name + " = " + value + "]");
    saveVariables();
}

void ScriptEngine::setAlias(const std::string &name, const std::string &command)
{
    if (name.empty()) {
        return;
    }
    m_aliases[name] = command;
    logScriptInfo("[ALIAS: " + name + " -> " + command + "]");
    saveAliases();
}

void ScriptEngine::setAction(const std::string &pattern, const std::string &command)
{
    if (pattern.empty()) {
        return;
    }
    ScriptActionType type = ScriptActionType::Wildcard;
    if (pattern.find_first_of("^$.()[]{}?+|") != std::string::npos) {
        type = ScriptActionType::Regex;
    }

    auto compiled = std::make_shared<CompiledScriptAction>(ScriptAction{type, pattern, command});
    m_actions[pattern] = compiled;
    m_actionMap.clear();
    for (const auto &pair : m_actions) {
        m_actionMap.emplace(pair.second->hint, pair.second);
    }
    logScriptInfo("[ACTION: " + pattern + " -> " + command + "]");
    saveActions();
}

bool ScriptEngine::removeVariable(const std::string &name)
{
    if (m_variables.erase(name)) {
        saveVariables();
        return true;
    }
    return false;
}

bool ScriptEngine::removeAlias(const std::string &name)
{
    if (m_aliases.erase(name)) {
        saveAliases();
        return true;
    }
    return false;
}

bool ScriptEngine::removeAction(const std::string &pattern)
{
    if (m_actions.erase(pattern)) {
        m_actionMap.clear();
        for (const auto &pair : m_actions) {
            m_actionMap.emplace(pair.second->hint, pair.second);
        }
        saveActions();
        return true;
    }
    return false;
}

std::vector<const ScriptAction *> ScriptEngine::getAllActions() const
{
    std::vector<const ScriptAction *> result;
    for (const auto &pair : m_actions) {
        result.push_back(&pair.second->action);
    }
    return result;
}

void ScriptEngine::saveVariables()
{
    QVariantMap data;
    for (const auto &pair : m_variables) {
        data[mmqt::toQStringUtf8(pair.first)] = mmqt::toQStringUtf8(pair.second);
    }
    setConfig().variables.setData(data);
}

void ScriptEngine::saveAliases()
{
    QVariantMap data;
    for (const auto &pair : m_aliases) {
        data[mmqt::toQStringUtf8(pair.first)] = mmqt::toQStringUtf8(pair.second);
    }
    setConfig().aliases.setData(data);
}

void ScriptEngine::saveActions()
{
    QVariantMap data;
    for (const auto &pair : m_actions) {
        data[mmqt::toQStringUtf8(pair.first)] = mmqt::toQStringUtf8(pair.second->action.command);
    }
    setConfig().actions.setData(data);
}

std::string ScriptEngine::wildcardToRegex(const std::string &wildcard)
{
    std::string result = "^";
    for (size_t i = 0; i < wildcard.size(); ++i) {
        char c = wildcard[i];
        if (c == '*') {
            result += "(.*)";
        } else if (std::string("^$.()[]{}?+|\\").find(c) != std::string::npos) {
            result += "\\";
            result += c;
        } else {
            result += c;
        }
    }
    result += "$";
    return result;
}

std::string ScriptEngine::substitute(const std::string &command,
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

void ScriptEngine::processServerFeed(StringView line)
{
    if (line.empty()) {
        return;
    }

    auto runMatch = [&](const std::shared_ptr<CompiledScriptAction> &compiled) {
        if (!compiled->regex) {
            return;
        }

        std::cmatch match;
        const std::string_view sv = line.getStdStringView();
        if (std::regex_search(sv.data(), sv.data() + sv.size(), match, *compiled->regex)) {
            std::vector<std::string> captures;
            captures.reserve(match.size());
            for (size_t i = 0; i < match.size(); ++i) {
                captures.emplace_back(match[i].str());
            }

            m_captures = captures;
            logScriptInfo("[TRIGGERED: " + compiled->action.pattern + "]");
            executeScript(substitute(compiled->action.command, captures));
        }
    };

    const char firstChar = line.firstChar();
    auto range = m_actionMap.equal_range(firstChar);
    for (auto it = range.first; it != range.second; ++it) {
        runMatch(it->second);
    }

    if (firstChar != 0) {
        range = m_actionMap.equal_range(0);
        for (auto it = range.first; it != range.second; ++it) {
            runMatch(it->second);
        }
    }
}

bool ScriptEngine::processUserInput(std::string &input)
{
    if (input.empty()) {
        return false;
    }

    // Check for aliases first
    StringView sv{input};
    std::string firstWord = sv.takeFirstWord().toStdString();
    auto it = m_aliases.find(firstWord);
    if (it != m_aliases.end()) {
        std::string rest = sv.toStdString();
        std::vector<std::string> args = {input};
        // Simple split for alias arguments %1..%9
        StringView restSv{rest};
        while (!restSv.isEmpty()) {
            args.push_back(restSv.takeFirstWord().toStdString());
        }

        std::string expanded = substitute(it->second, args);
        input = expanded;
        return true;
    }

    // Variable expansion
    std::string expanded = expandVariables(input);
    if (expanded != input) {
        input = expanded;
        return true;
    }

    return false;
}

std::string ScriptEngine::expandVariables(const std::string &input)
{
    std::string result;
    result.reserve(input.size());
    for (size_t i = 0; i < input.size(); ++i) {
        if (input[i] == '$') {
            size_t j = i + 1;
            while (j < input.size() && (std::isalnum(input[j]) || input[j] == '_')) {
                j++;
            }
            if (j > i + 1) {
                std::string varName = input.substr(i + 1, j - i - 1);
                auto it = m_variables.find(varName);
                if (it != m_variables.end()) {
                    result += it->second;
                } else {
                    result += "$" + varName;
                }
                i = j - 1;
            } else {
                result += '$';
            }
        } else if (input[i] == '%' && i + 1 < input.size() && std::isdigit(input[i + 1])) {
            size_t idx = static_cast<size_t>(input[i + 1] - '0');
            if (idx < m_captures.size()) {
                result += m_captures[idx];
            }
            i++;
        } else {
            result += input[i];
        }
    }
    return result;
}

std::vector<std::string> ScriptEngine::parseArguments(StringView &input)
{
    std::vector<std::string> args;
    input.trimLeft();
    while (!input.isEmpty()) {
        if (input.firstChar() == '{') {
            static_cast<void>(input.takeFirstLetter());
            int depth = 1;
            std::string arg;
            while (!input.isEmpty() && depth > 0) {
                char c = input.takeFirstLetter();
                if (c == '{') {
                    depth++;
                } else if (c == '}') {
                    depth--;
                }

                if (depth > 0) {
                    arg += c;
                }
            }
            args.push_back(arg);
        } else {
            args.push_back(input.takeFirstWord().toStdString());
        }
        input.trimLeft();
    }
    return args;
}

void ScriptEngine::executeScript(const std::string &script)
{
    std::string expanded = expandVariables(script);

    // Split by unescaped semicolon
    std::vector<std::string> commands;
    std::string currentCmd;
    bool escaped = false;
    int braceDepth = 0;

    for (size_t i = 0; i < expanded.size(); ++i) {
        char c = expanded[i];
        if (escaped) {
            currentCmd += c;
            escaped = false;
        } else if (c == '\\') {
            escaped = true;
        } else if (c == '{') {
            braceDepth++;
            currentCmd += c;
        } else if (c == '}') {
            braceDepth--;
            currentCmd += c;
        } else if (c == ';' && braceDepth == 0) {
            StringView cmdView{currentCmd};
            cmdView.trim();
            if (!cmdView.isEmpty()) {
                commands.push_back(cmdView.toStdString());
            }
            currentCmd.clear();
        } else {
            currentCmd += c;
        }
    }
    StringView cmdView{currentCmd};
    cmdView.trim();
    if (!cmdView.isEmpty()) {
        commands.push_back(cmdView.toStdString());
    }

    for (const auto &cmd : commands) {
        if (m_executeCallback) {
            m_executeCallback(cmd);
        }
    }
}

bool ScriptEngine::evaluateExpression(const std::string &expr)
{
    std::string expanded = expandVariables(expr);
    // Simple evaluation: non-empty, non-zero
    if (expanded.empty()) {
        return false;
    }
    if (expanded == "0" || expanded == "false") {
        return false;
    }

    // Basic math comparison
    std::regex relOp(R"(\s*(.+?)\s*(==|!=|>=|<=|>|<)\s*(.+)\s*)");
    std::smatch match;
    if (std::regex_match(expanded, match, relOp)) {
        std::string left = match[1].str();
        std::string op = match[2].str();
        std::string right = match[3].str();

        try {
            double lVal = std::stod(left);
            double rVal = std::stod(right);
            if (op == "==") {
                return lVal == rVal;
            }
            if (op == "!=") {
                return lVal != rVal;
            }
            if (op == ">=") {
                return lVal >= rVal;
            }
            if (op == "<=") {
                return lVal <= rVal;
            }
            if (op == ">") {
                return lVal > rVal;
            }
            if (op == "<") {
                return lVal < rVal;
            }
        } catch (...) {
            // String comparison
            if (op == "==") {
                return left == right;
            }
            if (op == "!=") {
                return left != right;
            }
        }
    }

    return true;
}

void ScriptEngine::logScriptError(const std::string &error)
{
    if (m_logCallback) {
        m_logCallback("Script Error: " + error);
    } else {
        MMLOG_ERROR() << "Script Error:" << error;
    }
}

void ScriptEngine::logScriptInfo(const std::string &info)
{
    if (m_logCallback) {
        m_logCallback(info);
    } else {
        qInfo() << "Script Info:" << mmqt::toQStringUtf8(info);
    }
}
