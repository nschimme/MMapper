// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "../client/Hotkey.h"
#include "../client/HotkeyManager.h"
#include "../configuration/configuration.h"
#include "../global/CaseUtils.h"
#include "../global/TextUtils.h"
#include "../syntax/SyntaxArgs.h"
#include "../syntax/TreeParser.h"
#include "AbstractParser-Utils.h"
#include "abstractparser.h"

#include <algorithm>
#include <ostream>
#include <sstream>
#include <vector>

class NODISCARD ArgHotkeyName final : public syntax::IArgument
{
private:
    NODISCARD syntax::MatchResult virt_match(const syntax::ParserInput &input,
                                             syntax::IMatchErrorLogger *) const final;

    std::ostream &virt_to_stream(std::ostream &os) const final;
};

syntax::MatchResult ArgHotkeyName::virt_match(const syntax::ParserInput &input,
                                              syntax::IMatchErrorLogger * /*logger */) const
{
    if (input.empty()) {
        return syntax::MatchResult::failure(input);
    }

    return syntax::MatchResult::success(1, input, Value{input.front()});
}

std::ostream &ArgHotkeyName::virt_to_stream(std::ostream &os) const
{
    return os << "<key>";
}

namespace {
std::string getInstructionalError(std::string_view keyCombo)
{
    std::string error = "Error: ";

    std::vector<std::string> parts;
    {
        std::string s(keyCombo);
        size_t start = 0;
        size_t end = s.find('+');
        while (true) {
            std::string part = s.substr(start, end - start);
            // Trim
            part.erase(0, part.find_first_not_of(" \t\n\r"));
            part.erase(part.find_last_not_of(" \t\n\r") + 1);
            if (!part.empty()) {
                parts.push_back(std::move(part));
            }
            if (end == std::string::npos) break;
            start = end + 1;
            end = s.find('+', start);
        }
    }

    std::string unrecognized;
    auto validKeys = Hotkey::getAvailableKeyNames();

    for (const auto &part : parts) {
        const std::string p = toUpperUtf8(part);
        if (p == "CTRL" || p == "SHIFT" || p == "ALT" || p == "META" || p == "CONTROL" || p == "CMD"
            || p == "COMMAND") {
            continue;
        }

        bool found = false;
        for (const auto &vk : validKeys) {
            if (p == toUpperUtf8(vk)) {
                found = true;
                break;
            }
        }

        if (!found) {
            unrecognized = part;
            break;
        }
    }

    if (unrecognized.empty() && !keyCombo.empty()) {
        // Must be missing a base key (e.g., "CTRL+")
        error += "\"";
        error += keyCombo;
        error += "\" is missing a valid key.\n";
    } else if (!unrecognized.empty()) {
        const std::string up = toUpperUtf8(unrecognized);
        if (up == "COMMAND" || up == "CMD") {
            error += "\"COMMAND\" is not a recognized modifier.\n";
        } else {
            error += "\"";
            error += unrecognized;
            error += "\" is not recognized.\n";
        }
    } else {
        error += "Invalid key combo.\n";
    }

    error += "Valid modifiers: CTRL, SHIFT, ALT, META\n";
    error += "Valid keys include: F1-F12, 0-9, UP, DOWN, etc.\n";
    error += "Try: \"-hotkey bind CTRL+G 75\"";
    return error;
}
} // namespace

void AbstractParser::parseHotkey(StringView input)
{
    using namespace ::syntax;
    static const auto abb = syntax::abbrevToken;

    // -hotkey bind <key_combo> <command>
    auto bindHotkey = Accept(
        [this](User &user, const Pair *const args) {
            auto &os = user.getOstream();
            const auto v = getAnyVectorReversed(args);

            const std::string keyName = v[1].getString();
            const std::string cmdStr = v[2].isVector() ? concatenate_unquoted(v[2].getVector())
                                                       : v[2].getString();

            Hotkey hk(keyName);
            if (!hk.isValid()) {
                os << getInstructionalError(keyName) << "\n";
                return;
            }

            if (m_hotkeyManager.setHotkey(hk, cmdStr)) {
                os << "Hotkey bound: [" << hk.serialize() << "] -> "
                   << cmdStr << "\n";
                send_ok(os);
            } else {
                os << "Failed to bind hotkey.\n";
            }
        },
        "bind hotkey");

    // -hotkey list
    auto listHotkeys = Accept(
        [this](User &user, const Pair *) {
            auto &os = user.getOstream();
            auto hotkeys = m_hotkeyManager.getAllHotkeys();
            std::sort(hotkeys.begin(), hotkeys.end(), [](const auto &a, const auto &b) {
                return a.first.serialize() < b.first.serialize();
            });

            if (hotkeys.empty()) {
                os << "No hotkeys configured.\n";
            } else {
                os << "Active Hotkeys:\n";
                for (const auto &[hk, cmd] : hotkeys) {
                    os << "  [" << hk.serialize() << "] -> " << cmd << "\n";
                }
                os << "Total: " << hotkeys.size() << "\n";
            }
            send_ok(os);
        },
        "list hotkeys");

    // -hotkey unbind <key_combo>
    auto unbindHotkey = Accept(
        [this](User &user, const Pair *const args) {
            auto &os = user.getOstream();
            const auto v = getAnyVectorReversed(args);
            const std::string keyName = v[1].getString();

            Hotkey hk(keyName);
            if (!hk.isValid()) {
                os << getInstructionalError(keyName) << "\n";
                return;
            }

            if (m_hotkeyManager.hasHotkey(hk)) {
                m_hotkeyManager.removeHotkey(hk);
                os << "Hotkey unbound: [" << hk.serialize() << "]\n";
                send_ok(os);
            } else {
                os << "No hotkey configured for: [" << hk.serialize()
                   << "]\n";
            }
        },
        "unbind hotkey");

    // Build syntax tree
    auto bindSyntax = buildSyntax(abb("bind"),
                                  TokenMatcher::alloc<ArgHotkeyName>(),
                                  TokenMatcher::alloc<ArgRest>(),
                                  bindHotkey);

    auto listSyntax = buildSyntax(abb("list"), listHotkeys);

    auto unbindSyntax = buildSyntax(abb("unbind"),
                                    TokenMatcher::alloc<ArgHotkeyName>(),
                                    unbindHotkey);

    // Basic syntax help trigger (if no arguments or invalid)
    auto helpFn = [this](User &user, const Pair *) {
        auto &os = user.getOstream();
        os << "Basic syntax help:\n"
           << "  -hotkey bind <key_combo> <command>\n"
           << "  -hotkey list\n"
           << "  -hotkey unbind <key_combo>\n"
           << "\n"
           << "Valid modifiers: CTRL, SHIFT, ALT, META\n"
           << "Valid keys:\n";

        // Programmatically list all valid keys
        auto keys = Hotkey::getAvailableKeyNames();
        std::sort(keys.begin(), keys.end());
        os << "  ";
        for (size_t i = 0; i < keys.size(); ++i) {
            os << keys[i] << (i == keys.size() - 1 ? "" : ", ");
            if ((i + 1) % 8 == 0)
                os << "\n  ";
        }
        os << "\n";
        send_ok(os);
    };

    auto hotkeyUserSyntax = buildSyntax(bindSyntax,
                                        listSyntax,
                                        unbindSyntax,
                                        Accept(helpFn, "help"));

    eval("hotkey", hotkeyUserSyntax, input);
}
