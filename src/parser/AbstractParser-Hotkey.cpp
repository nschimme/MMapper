// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "../client/HotkeyManager.h"
#include "../configuration/configuration.h"
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
QString getInstructionalError(const QString &keyCombo)
{
    QString error = "Error: ";
    QStringList parts = keyCombo.split('+', Qt::SkipEmptyParts);

    QString unrecognized;
    for (const auto &part : parts) {
        QString p = part.trimmed().toUpper();
        if (p == "CTRL" || p == "SHIFT" || p == "ALT" || p == "META") {
            continue;
        }

        bool found = false;
#define X_CHECK(id, str, key, numpad) \
    if (p == str) \
        found = true;
        XFOREACH_HOTKEY_BASE_KEYS(X_CHECK)
#undef X_CHECK

        if (!found) {
            unrecognized = part.trimmed();
            break;
        }
    }

    if (unrecognized.isEmpty() && !keyCombo.isEmpty()) {
        // Must be missing a base key (e.g., "CTRL+")
        error += "\"" + keyCombo + "\" is missing a valid key.\n";
    } else if (!unrecognized.isEmpty()) {
        if (unrecognized.toUpper() == "COMMAND" || unrecognized.toUpper() == "CMD") {
            error += "\"COMMAND\" is not a recognized modifier.\n";
        } else {
            error += "\"" + unrecognized + "\" is not recognized.\n";
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

            const QString keyName = mmqt::toQStringUtf8(v[1].getString());
            const std::string cmdStr = v[2].isVector() ? concatenate_unquoted(v[2].getVector())
                                                       : v[2].getString();

            Hotkey hk = Hotkey::deserialize(keyName);
            if (!hk.isValid()) {
                os << mmqt::toStdStringUtf8(getInstructionalError(keyName)) << "\n";
                return;
            }

            if (m_hotkeyManager.setHotkey(hk, cmdStr)) {
                os << "Hotkey bound: [" << mmqt::toStdStringUtf8(hk.serialize()) << "] -> " << cmdStr
                   << "\n";
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
            std::sort(hotkeys.begin(), hotkeys.end());

            if (hotkeys.empty()) {
                os << "No hotkeys configured.\n";
            } else {
                os << "Active Hotkeys:\n";
                for (const auto &[key, cmd] : hotkeys) {
                    os << "  [" << mmqt::toStdStringUtf8(key) << "] -> " << cmd << "\n";
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
            const QString keyName = mmqt::toQStringUtf8(v[1].getString());

            Hotkey hk = Hotkey::deserialize(keyName);
            if (!hk.isValid()) {
                os << mmqt::toStdStringUtf8(getInstructionalError(keyName)) << "\n";
                return;
            }

            if (m_hotkeyManager.hasHotkey(hk)) {
                m_hotkeyManager.removeHotkey(hk);
                os << "Hotkey unbound: [" << mmqt::toStdStringUtf8(hk.serialize()) << "]\n";
                send_ok(os);
            } else {
                os << "No hotkey configured for: [" << mmqt::toStdStringUtf8(hk.serialize()) << "]\n";
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
        auto keys = HotkeyManager::getAvailableKeyNames();
        std::sort(keys.begin(), keys.end());
        os << "  ";
        for (size_t i = 0; i < keys.size(); ++i) {
            os << mmqt::toStdStringUtf8(keys[i]) << (i == keys.size() - 1 ? "" : ", ");
            if ((i + 1) % 8 == 0)
                os << "\n  ";
        }
        os << "\n";
        send_ok(os);
    };

    auto hotkeyUserSyntax = buildSyntax(bindSyntax, listSyntax, unbindSyntax, Accept(helpFn, "help"));

    eval("hotkey", hotkeyUserSyntax, input);
}
