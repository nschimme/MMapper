#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: 'Ethorondil' <ethorondil@gmail.com> (Elval)

#include <cassert>
#include <QString>
#include <QtCore>

#include "../expandoracommon/room.h"

class Room;

enum class PatternKindsEnum { NONE, DESC, DYN_DESC, NAME, NOTE, EXITS, FLAGS, ALL };
static constexpr const auto PATTERN_KINDS_LENGTH = static_cast<size_t>(PatternKindsEnum::ALL) + 1u;
static_assert(PATTERN_KINDS_LENGTH == 8);

class RoomFilter
{
public:
    RoomFilter() = default;
    explicit RoomFilter(const QString &pattern,
                        const Qt::CaseSensitivity &cs,
                        const PatternKindsEnum kind)
        : pattern(pattern)
        , cs(cs)
        , kind(kind)
    {}

    // Return false on parse failure
    static bool parseRoomFilter(const QString &line, RoomFilter &output);
    static const char *parse_help;

    bool filter(const Room *r) const;
    PatternKindsEnum patternKind() const { return kind; }

protected:
    QString pattern;
    Qt::CaseSensitivity cs;
    PatternKindsEnum kind = PatternKindsEnum::NONE;
};
