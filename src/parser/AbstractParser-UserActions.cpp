#include "../client/UserActionManager.h"
#include "../syntax/SyntaxArgs.h"
#include "../syntax/TreeParser.h"
#include "AbstractParser-Utils.h"
#include "abstractparser.h"

#include <algorithm>
#include <map>
#include <ostream>
#include <vector>

void AbstractParser::parseUserAction(StringView input)
{
    using namespace ::syntax;
    static const auto abb = syntax::abbrevToken;

    auto bindAction = Accept(
        [this](User &user, const Pair *const args) {
            auto &os = user.getOstream();
            const auto v = getAnyVectorReversed(args);

            UserActionType type = UserActionType::Regex;
            std::string pattern;
            std::string command;

            if (v.size() == 3) {
                // Branch 1: /action -type pattern command...
                std::string flag = v[0].getString();
                if (flag == "-starts") {
                    type = UserActionType::StartsWith;
                } else if (flag == "-ends") {
                    type = UserActionType::EndsWith;
                }
                pattern = v[1].getString();
                const Vector &cmdVec = v[2].getVector();
                if (cmdVec.empty()) {
                    os << "Syntax: action [-type] <pattern> <command>\n";
                    return;
                }
                command = concatenate_unquoted(cmdVec);
            } else if (v.size() == 2) {
                // Branch 2: /action pattern [command...]
                pattern = v[0].getString();
                const Vector &cmdVec = v[1].getVector();
                if (cmdVec.empty()) {
                    // Removal
                    if (m_userActionManager.removeAction(pattern)) {
                        os << "Action removed: " << pattern << "\n";
                        send_ok(os);
                    } else {
                        os << "No action found for: " << pattern << "\n";
                    }
                    return;
                }
                command = concatenate_unquoted(cmdVec);
            } else {
                os << "Internal error.\n";
                return;
            }

            if (m_userActionManager.setAction(type, pattern, command)) {
                os << "Action defined.\n";
                send_ok(os);
            } else {
                os << "Failed to define action.\n";
            }
        },
        "define action");

    auto listActions = Accept(
        [this](User &user, const Pair *) {
            auto &os = user.getOstream();
            auto actions = m_userActionManager.getAllActions();
            std::sort(actions.begin(), actions.end(), [](const auto *a, const auto *b) {
                return a->pattern < b->pattern;
            });

            if (actions.empty()) {
                os << "No actions defined.\n";
            } else {
                os << "Active Actions:\n";
                for (const auto *action : actions) {
                    std::string typeStr;
                    switch (action->type) {
                    case UserActionType::Regex:
                        typeStr = "regex ";
                        break;
                    case UserActionType::StartsWith:
                        typeStr = "starts";
                        break;
                    case UserActionType::EndsWith:
                        typeStr = "ends  ";
                        break;
                    }
                    os << "  [" << typeStr << "] " << action->pattern << " -> " << action->command
                       << "\n";
                }
                os << "Total: " << actions.size() << "\n";
            }
            send_ok(os);
        },
        "list actions");

    // Syntax:
    // /action [-type] pattern command -> define
    // /action pattern -> remove
    // /action -> list

    auto typeChoice = TokenMatcher::alloc<ArgChoice>(abb("-regex"), abb("-starts"), abb("-ends"));

    auto actionSyntax = buildSyntax(buildSyntax(typeChoice,
                                                TokenMatcher::alloc<ArgString>(),
                                                TokenMatcher::alloc<ArgRest>(),
                                                bindAction),
                                    buildSyntax(TokenMatcher::alloc<ArgString>(),
                                                TokenMatcher::alloc<ArgRest>(),
                                                bindAction),
                                    listActions);

    eval("action", actionSyntax, input);
}

void AbstractParser::parseUserUnaction(StringView /*input*/)
{
    // Deprecated, logic moved to parseUserAction
}
