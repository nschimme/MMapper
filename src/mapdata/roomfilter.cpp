// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: 'Elval' <ethorondil@gmail.com> (Elval)

#include "roomfilter.h"

#include "../global/StringView.h"
#include "../global/TextUtils.h"
#include "../parser/Abbrev.h"

#include <optional>

#include <QRegularExpression>
#include <QString>

NODISCARD static QString escapeRegex(const QString &str)
{
    static const QRegularExpression metacharactersRx(R"([.*+?^${}()|\[\]\\])");
    QString result = str;
    int offset = 0;
    auto it = metacharactersRx.globalMatch(str);
    while (it.hasNext()) {
        auto match = it.next();
        result.insert(match.capturedStart() + offset, '\\');
        ++offset;
    }
    return result;
}

NODISCARD static QRegularExpression createRegex(const std::string_view input,
                                                const Qt::CaseSensitivity cs,
                                                const bool regex)
{
    QRegularExpression::PatternOptions options = QRegularExpression::NoPatternOption;
    if (cs == Qt::CaseInsensitive) {
        options |= QRegularExpression::CaseInsensitiveOption;
    }
    const auto makeRegex = [&input, &regex]() -> QString {
        if (input.empty()) {
            return QStringLiteral(R"(^$)");
        }

        if (regex) {
            return mmqt::toQStringUtf8(input);
        }

        QString pattern = escapeRegex(mmqt::toQStringUtf8(input));
        static const QRegularExpression whitespaceRx(QStringLiteral(R"(\s+)"));
        pattern.replace(whitespaceRx, QStringLiteral(R"(\s+)"));
        return QStringLiteral(".*") + pattern + QStringLiteral(".*");
    };
    return QRegularExpression(makeRegex(), options);
}

RoomFilter::RoomFilter(const std::string_view sv,
                       const Qt::CaseSensitivity cs,
                       const bool regex,
                       const PatternKindsEnum kind)
    : m_regex(createRegex(sv, cs, regex))
    , m_kind(kind)
{}

const char *const RoomFilter::parse_help
    = "Parse error; format is: [-[r|regex]] -(name|desc|contents|note|exits|area|all|clear) pattern\n"
      "  -r, -regex: Treat the pattern as a regular expression.\n"
      "  -name: Search by room name (default if no flag is given).\n"
      "  -desc: Search by room description.\n"
      "  -contents: Search by room contents.\n"
      "  -note: Search by room note.\n"
      "  -exits: Search by exit names.\n"
      "  -flags: Search by room or exit flags.\n"
      "  -area: Search by area name.\n"
      "  -all: Search across all fields.\n"
      "  -clear: Clear the previous search results.\n";

std::optional<RoomFilter> RoomFilter::parseRoomFilter(const std::string_view line)
{
    // REVISIT: rewrite this using the new syntax tree model.
    auto view = StringView{line}.trim();
    bool regex = false;
    PatternKindsEnum kind = PatternKindsEnum::NAME;
    if (view.isEmpty()) {
        return std::nullopt;
    } else if (view.takeFirstLetter() != char_consts::C_MINUS_SIGN) {
        return RoomFilter{line, Qt::CaseInsensitive, regex, kind};
    }

    auto first = view.takeFirstWord();
    if (Abbrev("regex", 1).matches(first)) {
        regex = true;
        if (view.isEmpty()) {
            // Require arguments beyond "-regex" or "-r"
            return std::nullopt;
        } else if (view.takeFirstLetter() != char_consts::C_MINUS_SIGN) {
            return RoomFilter{view.toStdString(), Qt::CaseInsensitive, regex, kind};
        }
        first = view.takeFirstWord();
    }

    const auto opt = [&first]() -> std::optional<PatternKindsEnum> {
        if (Abbrev("desc", 1).matches(first)) {
            return PatternKindsEnum::DESC;
        } else if (Abbrev("contents", 2).matches(first)) {
            return PatternKindsEnum::CONTENTS;
        } else if (Abbrev("name", 2).matches(first)) {
            return PatternKindsEnum::NAME;
        } else if (Abbrev("exits", 1).matches(first)) {
            return PatternKindsEnum::EXITS;
        } else if (Abbrev("note", 1).matches(first)) {
            return PatternKindsEnum::NOTE;
        } else if (Abbrev("area", 2).matches(first)) {
            return PatternKindsEnum::AREA;
        } else if (Abbrev("all", 1).matches(first)) {
            return PatternKindsEnum::ALL;
        } else if (Abbrev("clear", 1).matches(first)) {
            return PatternKindsEnum::NONE;
        } else if (Abbrev("flags", 1).matches(first)) {
            return PatternKindsEnum::FLAGS;
        } else {
            return std::nullopt;
        }
    }();
    if (!opt.has_value()) {
        return std::nullopt;
    }

    kind = opt.value();
    if (kind != PatternKindsEnum::NONE) {
        // Require pattern text in addition to arguments
        if (view.empty()) {
            return std::nullopt;
        }
    }
    return RoomFilter{view.toStdString(), Qt::CaseInsensitive, regex, kind};
}

bool RoomFilter::filter_kind(const RawRoom &r, const PatternKindsEnum pat) const
{
    switch (pat) {
    case PatternKindsEnum::ALL:
        break;

    case PatternKindsEnum::DESC:
        return matches(r.getDescription());

    case PatternKindsEnum::CONTENTS:
        return matches(r.getContents());

    case PatternKindsEnum::NAME:
        return matches(r.getName());

    case PatternKindsEnum::NOTE:
        return matches(r.getNote());

    case PatternKindsEnum::EXITS:
        for (const auto &e : r.getExits()) {
            if (matches(e.getDoorName())) {
                return true;
            }
        }
        return false;

    case PatternKindsEnum::FLAGS: {
        for (const auto &e : r.getExits()) {
            if (this->matchesAny(e.getDoorFlags()) //
                || this->matchesAny(e.getExitFlags())) {
                return true;
            }
        }

        return this->matchesAny(r.getMobFlags())            //
               || this->matchesAny(r.getLoadFlags())        //
               || this->matchesDefined(r.getLightType())    //
               || this->matchesDefined(r.getSundeathType()) //
               || this->matchesDefined(r.getPortableType()) //
               || this->matchesDefined(r.getRidableType())  //
               || this->matchesDefined(r.getAlignType());
    }

    case PatternKindsEnum::AREA:
        return matches(r.getArea());

    case PatternKindsEnum::NONE:
        return false;
    }

    throw std::invalid_argument("pat");
};

bool RoomFilter::filter(const RawRoom &r) const
{
    if (m_kind != PatternKindsEnum::ALL) {
        return filter_kind(r, m_kind);
    }

    // NOTE: using C-style array allows static assert on the number of elements, but std::array doesn't
    // in this case because the compiler will report excess elements but not too few elements.
    // Alternate: make this a std::vector and then either do a regular assert, or remove the assert.
    static constexpr const PatternKindsEnum ALL_KINDS[]{PatternKindsEnum::DESC,
                                                        PatternKindsEnum::CONTENTS,
                                                        PatternKindsEnum::NAME,
                                                        PatternKindsEnum::NOTE,
                                                        PatternKindsEnum::EXITS,
                                                        PatternKindsEnum::FLAGS,
                                                        PatternKindsEnum::AREA};
    static constexpr const size_t ALL_KINDS_SIZE = sizeof(ALL_KINDS) / sizeof(ALL_KINDS[0]);
    static_assert(ALL_KINDS_SIZE == PATTERN_KINDS_LENGTH - 2); // excludes NONE and ALL.
    for (const auto &pat : ALL_KINDS) {
        if (filter_kind(r, pat)) {
            return true;
        }
    }
    return false;
}
