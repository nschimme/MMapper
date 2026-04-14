// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Nils Schimmelmann <nschimme@gmail.com> (Jahara)

#include "Action.h"

#include <regex>

IAction::~IAction() = default;

void IAction::match(const StringView input) const
{
    return virt_match(input);
}

void StartsWithAction::virt_match(const StringView input) const
{
    if (input.startsWith(m_match)) {
        m_callback(input, {input});
    }
}

void EndsWithAction::virt_match(const StringView input) const
{
    if (input.endsWith(m_match)) {
        m_callback(input, {input});
    }
}

NODISCARD static std::regex createRegex(const std::string &pattern)
{
    return std::regex(pattern, std::regex::optimize);
}

RegexAction::RegexAction(const std::string &pattern, const ActionCallback &callback)
    : m_regex{createRegex(pattern)}
    , m_callback{callback}
{}

void RegexAction::virt_match(const StringView input) const
{
    std::cmatch match;
    if (std::regex_search(input.begin(), input.end(), m_regex, match)) {
        std::vector<StringView> captures;
        captures.reserve(match.size());
        for (size_t i = 0; i < match.size(); ++i) {
            const auto &sub = match[i];
            if (sub.matched) {
                captures.emplace_back(
                    std::string_view(sub.first, static_cast<size_t>(sub.length())));
            } else {
                captures.emplace_back();
            }
        }
        m_callback(input, captures);
    }
}
