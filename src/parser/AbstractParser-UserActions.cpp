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

            // action [-regex|-starts|-ends] <pattern> <command>
            // v[0] = "action"
            // v[1] = type flag (optional)
            // v[2] = pattern
            // v[3] = command

            UserActionType type = UserActionType::Regex;
            size_t patternIdx = 1;

            if (v.size() > 2 && v[1].isString()) {
                std::string flag = v[1].getString();
                if (flag == "-regex") {
                    type = UserActionType::Regex;
                    patternIdx = 2;
                } else if (flag == "-starts") {
                    type = UserActionType::StartsWith;
                    patternIdx = 2;
                } else if (flag == "-ends") {
                    type = UserActionType::EndsWith;
                    patternIdx = 2;
                }
            }

            if (v.size() <= patternIdx) {
                os << "Internal error.\n";
                return;
            }

            std::string pattern = v[patternIdx].getString();

            // If no command provided, it's a remove request
            if (v.size() <= patternIdx + 1) {
                if (m_userActionManager.removeAction(pattern)) {
                    os << "Action removed: " << pattern << "\n";
                    send_ok(os);
                } else {
                    os << "No action found for: " << pattern << "\n";
                }
                return;
            }

            std::string command = v[patternIdx + 1].isVector()
                                      ? concatenate_unquoted(v[patternIdx + 1].getVector())
                                      : v[patternIdx + 1].getString();

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
    auto typeMatcher = TokenMatcher::alloc<ArgOptionalToken>(typeChoice);

    auto actionSyntax = buildSyntax(buildSyntax(typeMatcher,
                                                TokenMatcher::alloc<ArgString>(),
                                                TokenMatcher::alloc<ArgRest>(),
                                                bindAction),
                                    buildSyntax(TokenMatcher::alloc<ArgString>(), bindAction),
                                    listActions);

    eval("action", actionSyntax, input);
}

void AbstractParser::parseUserUnaction(StringView /*input*/)
{
    // Deprecated, logic moved to parseUserAction
}
